// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>
#include "nosTrack/Track_generated.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <fstream>
#include <filesystem>
#include <vector>
#include <cmath>
#include <iomanip>

#include <nosMath/CoordinateFrameConversion.hpp>

namespace nos::track
{

NOS_REGISTER_NAME(OutputDirectory);
NOS_REGISTER_NAME(ImageResolution);
NOS_REGISTER_NAME(SourceFrame);
NOS_REGISTER_NAME(Record);
NOS_REGISTER_NAME(MinOffFrames);
NOS_REGISTER_NAME(FrameCount);
NOS_REGISTER_NAME(RecordingFrame);

struct RecordedFrame
{
	glm::vec3 Location;
	glm::vec3 Rotation; // Euler degrees in the SourceFrame's convention.
	float FOV;
	float Zoom;
	float Focus;
	float RenderRatio;
	glm::vec2 SensorSize;
	float PixelAspectRatio;
	float NodalOffset;
	float FocusDistance;
	float K1;
	float K2;
	glm::vec2 CenterShift;
	float DistortionScale;
	std::string Timecode;
	uint32_t FrameNumber;
};

struct RecordTrackCOLMAPContext : NodeContext
{
	std::string OutputDir;
	nosVec2u ImageResolution = {1920, 1080};
	nos::math::Frame SourceFrame = nos::math::UNREAL_SYSTEM;
	bool Recording = false;
	uint32_t ConsecutiveOffFrames = 0;
	bool LastRequestRecord = false;
	std::string LastError;
	std::vector<RecordedFrame> Frames;
	nosVec2u DeltaSeconds{}; // {numerator, denominator}; 0/0 if not in fixed-step mode

	nosResult OnCreate(nosFbNodePtr node) override
	{
		if (node->pins())
		{
			for (auto* pin : *node->pins())
			{
				auto name = nos::Name(pin->name()->c_str());
				if (flatbuffers::IsFieldPresent(pin, fb::Pin::VT_DATA))
				{
					nosBuffer value = {.Data = (void*)pin->data()->data(), .Size = pin->data()->size()};
					OnPinValueChanged(name, *pin->id(), value);
				}
			}
		}
		UpdateStatus();
		return NOS_RESULT_SUCCESS;
	}

	bool StartRecording()
	{
		std::string error;
		if (!CanStartRecording(error))
		{
			LastError = std::move(error);
			UpdateStatus();
			return false;
		}
		LastError.clear();
		Frames.clear();
		Recording = true;
		ConsecutiveOffFrames = 0;
		UpdateFrameCountPin();
		UpdateRecordingFramePin();
		UpdateStatus();
		nosEngine.LogI("RecordTrackCOLMAP: Recording started");
		return true;
	}

	void StopRecording()
	{
		Recording = false;
		nosEngine.LogI("RecordTrackCOLMAP: Recording stopped (%zu frames in buffer)", Frames.size());
		if (!Frames.empty())
			WriteFiles();
		Frames.clear();
		UpdateFrameCountPin();
		UpdateRecordingFramePin();
		UpdateStatus();
	}

	void OnPinValueChanged(nos::Name pinName, uuid const& pinId, nosBuffer val) override
	{
		if (pinName == NSN_OutputDirectory)
		{
			OutputDir = static_cast<const char*>(val.Data);
			LastError.clear();
			UpdateStatus();
		}
		else if (pinName == NSN_ImageResolution)
			ImageResolution = *(nosVec2u*)val.Data;
		else if (pinName == NSN_SourceFrame)
			SourceFrame = *(nos::math::Frame*)val.Data;
	}

	bool CanStartRecording(std::string& outError)
	{
		if (OutputDir.empty())
		{
			outError = "Set output directory";
			return false;
		}

		std::filesystem::path outDir = nos::Utf8ToPath(OutputDir);
		try
		{
			if (std::filesystem::exists(outDir) && !std::filesystem::is_empty(outDir))
			{
				outError = "Target folder is not empty";
				return false;
			}
		}
		catch (std::filesystem::filesystem_error& e)
		{
			nosEngine.LogE("RecordTrackCOLMAP: %s", e.what());
			outError = e.what();
			return false;
		}
		return true;
	}

	void UpdateFrameCountPin()
	{
		uint32_t count = (uint32_t)Frames.size();
		SetPinValue(NSN_FrameCount, nosBuffer{.Data = &count, .Size = sizeof(count)});
	}

	void UpdateRecordingFramePin()
	{
		uint32_t frame = Recording ? (uint32_t)Frames.size() : 0;
		SetPinValue(NSN_RecordingFrame, nosBuffer{.Data = &frame, .Size = sizeof(frame)});
	}

	void UpdateStatus()
	{
		if (!LastError.empty())
			SetNodeStatusMessage(LastError, fb::NodeStatusMessageType::FAILURE);
		else if (OutputDir.empty())
			SetNodeStatusMessage("Set output directory", fb::NodeStatusMessageType::WARNING);
		else if (Recording)
			SetNodeStatusMessage("Recording (" + std::to_string(Frames.size()) + " frames)", fb::NodeStatusMessageType::INFO);
		else
			SetNodeStatusMessage("Idle", fb::NodeStatusMessageType::INFO);
	}

	nosResult ExecuteNode(nos::NodeExecuteParams const& params) override
	{
		if (params.TimingMode == NOS_EXECUTION_TIMING_MODE_FIXED_STEP)
			DeltaSeconds = params.FixedStepTiming.DeltaSeconds;

		// Pass through Track input to output
		auto trackBuf = params.GetPinBuffer(NOS_NAME("InTrack"));
		SetPinValue(NOS_NAME("OutTrack"), trackBuf);

		// Drive recording state from the Record pin, with off-state debouncing to
		// ride out brief glitches in the upstream signal (e.g. SDI bit flips on a
		// camera-derived recording flag). Start happens immediately on a rising
		// edge; stop only after MinOffFrames consecutive false frames.
		const bool requestRecord = *params.GetPinValue<bool>(NSN_Record);
		const uint32_t minOffFrames = *params.GetPinValue<uint32_t>(NSN_MinOffFrames);

		const bool risingEdge = requestRecord && !LastRequestRecord;
		LastRequestRecord = requestRecord;

		if (risingEdge && !Recording)
			StartRecording();

		if (Recording)
		{
			if (requestRecord)
				ConsecutiveOffFrames = 0;
			else if (++ConsecutiveOffFrames >= std::max(1u, minOffFrames))
				StopRecording();
		}

		if (!Recording)
			return NOS_RESULT_SUCCESS;

		auto* trackData = flatbuffers::GetRoot<nos::track::Track>(trackBuf.Data);
		if (!trackData)
			return NOS_RESULT_SUCCESS;

		RecordedFrame frame{};
		if (const char* tc = params.GetPinValue<const char>(NOS_NAME_STATIC("Timecode")))
			frame.Timecode = tc;
		frame.FrameNumber = *params.GetPinValue<uint32_t>(NOS_NAME_STATIC("FrameNumber"));
		if (auto* loc = trackData->location())
			frame.Location = {loc->x(), loc->y(), loc->z()};
		if (auto* rot = trackData->rotation())
			frame.Rotation = {rot->x(), rot->y(), rot->z()};
		frame.FOV = trackData->fov();
		frame.Zoom = trackData->zoom();
		frame.Focus = trackData->focus();
		frame.RenderRatio = trackData->render_ratio();
		if (auto* ss = trackData->sensor_size())
			frame.SensorSize = {ss->x(), ss->y()};
		frame.PixelAspectRatio = trackData->pixel_aspect_ratio();
		frame.NodalOffset = trackData->nodal_offset();
		frame.FocusDistance = trackData->focus_distance();
		if (auto* ld = trackData->lens_distortion())
		{
			frame.K1 = ld->k1k2().x();
			frame.K2 = ld->k1k2().y();
			frame.CenterShift = {ld->center_shift().x(), ld->center_shift().y()};
			frame.DistortionScale = ld->distortion_scale();
		}
		Frames.push_back(frame);

		UpdateFrameCountPin();
		UpdateRecordingFramePin();
		UpdateStatus();

		return NOS_RESULT_SUCCESS;
	}

	void WriteFiles()
	{
		if (OutputDir.empty())
		{
			nosEngine.LogE("RecordTrackCOLMAP: Output directory is empty");
			return;
		}
		if (Frames.empty())
		{
			nosEngine.LogW("RecordTrackCOLMAP: No frames recorded");
			return;
		}

		std::filesystem::path outDir = nos::Utf8ToPath(OutputDir);
		try
		{
			if (!std::filesystem::exists(outDir))
				std::filesystem::create_directories(outDir);
		}
		catch (std::filesystem::filesystem_error& e)
		{
			nosEngine.LogE("RecordTrackCOLMAP: %s", e.what());
			return;
		}

		WriteCamerasTxt(outDir);
		WriteImagesTxt(outDir);
		WriteTimecodesTxt(outDir);
		WriteExtrasTxt(outDir);
		nosEngine.LogI("RecordTrackCOLMAP: Saved %zu frames to %s", Frames.size(), OutputDir.c_str());
	}

	void WriteExtrasTxt(const std::filesystem::path& outDir)
	{
		// Sidecar for Track fields that don't fit COLMAP's standard cameras.txt /
		// images.txt format. Keyed by IMAGE_ID so it pairs 1:1 with images.txt.
		auto path = outDir / "extras.txt";
		std::ofstream file(path);
		if (!file.is_open())
		{
			nosEngine.LogE("RecordTrackCOLMAP: Cannot open %s", nos::PathToUtf8(path).c_str());
			return;
		}
		const std::string frameName =
			std::string("up=") + nos::math::EnumNameSignedAxis(SourceFrame.up())
			+ " forward=" + nos::math::EnumNameSignedAxis(SourceFrame.forward())
			+ " handedness=" + nos::math::EnumNameHandedness(SourceFrame.handedness())
			+ " euler_order=" + nos::math::EnumNameEulerOrder(SourceFrame.euler().order())
			+ " euler_sign=" + std::to_string((int)SourceFrame.euler().sign_x())
			+ "," + std::to_string((int)SourceFrame.euler().sign_y())
			+ "," + std::to_string((int)SourceFrame.euler().sign_z());
		file << std::setprecision(12);
		file << "# Nodos Track sidecar paired with images.txt by IMAGE_ID.\n";
		file << "# Carries fields that don't fit COLMAP's cameras.txt/images.txt:\n";
		file << "#   - sensor_size in mm (cameras.txt only stores pixel WIDTH/HEIGHT)\n";
		file << "#   - original Euler rotation in degrees (avoids quaternion round-trip drift)\n";
		file << "#   - nodos-only fields with no COLMAP equivalent\n";
		file << "# SourceFrame: " << frameName << " (Euler convention used for ROT_X, ROT_Y, ROT_Z below).\n";
		file << "# IMAGE_ID, ZOOM, FOCUS, FOCUS_DISTANCE, RENDER_RATIO, NODAL_OFFSET, DISTORTION_SCALE, SENSOR_W_MM, SENSOR_H_MM, ROT_X, ROT_Y, ROT_Z\n";
		file << "# Number of entries: " << Frames.size() << "\n";
		for (size_t i = 0; i < Frames.size(); ++i)
		{
			const auto& f = Frames[i];
			file << (i + 1) << " "
				 << f.Zoom << " "
				 << f.Focus << " "
				 << f.FocusDistance << " "
				 << f.RenderRatio << " "
				 << f.NodalOffset << " "
				 << f.DistortionScale << " "
				 << f.SensorSize.x << " " << f.SensorSize.y << " "
				 << f.Rotation.x << " " << f.Rotation.y << " " << f.Rotation.z << "\n";
		}
	}

	void WriteTimecodesTxt(const std::filesystem::path& outDir)
	{
		// Skip the sidecar entirely if no frame carried a timecode -- keeps the
		// output minimal when the upstream graph isn't producing TC.
		bool any = false;
		for (auto& f : Frames)
			if (!f.Timecode.empty() || f.FrameNumber != 0) { any = true; break; }
		if (!any)
			return;

		auto path = outDir / "timecodes.txt";
		std::ofstream file(path);
		if (!file.is_open())
		{
			nosEngine.LogE("RecordTrackCOLMAP: Cannot open %s", nos::PathToUtf8(path).c_str());
			return;
		}
		double dt = (DeltaSeconds.y != 0) ? (double)DeltaSeconds.x / (double)DeltaSeconds.y : 0.0;
		file << "# Timecode sidecar paired with images.txt by IMAGE_ID.\n";
		file << "# First non-comment line: per-frame delta seconds (0 if recording wasn't in fixed-step timing).\n";
		file << "# IMAGE_ID, TIMECODE, FRAME_NUMBER\n";
		file << "# Number of entries: " << Frames.size() << "\n";
		file << std::setprecision(12) << dt << "\n";
		for (size_t i = 0; i < Frames.size(); ++i)
		{
			const auto& f = Frames[i];
			file << (i + 1) << " "
				 << (f.Timecode.empty() ? "-" : f.Timecode) << " "
				 << f.FrameNumber << "\n";
		}
	}

	float ComputeFocalLengthPixels(const RecordedFrame& frame) const
	{
		if (frame.FOV <= 0.0f)
			return static_cast<float>(ImageResolution.x);
		float fovRad = glm::radians(frame.FOV);
		return (ImageResolution.x * 0.5f) / std::tan(fovRad * 0.5f);
	}

	void WriteCamerasTxt(const std::filesystem::path& outDir)
	{
		auto path = outDir / "cameras.txt";
		std::ofstream file(path);
		if (!file.is_open())
		{
			nosEngine.LogE("RecordTrackCOLMAP: Cannot open %s", nos::PathToUtf8(path).c_str());
			return;
		}

		file << std::setprecision(12);
		file << "# COLMAP camera intrinsics. Standard format (colmap.github.io/format.html).\n";
		file << "# OPENCV model: PARAMS = fx, fy, cx, cy, k1, k2, p1, p2 (pixels).\n";
		file << "# Camera list with one line of data per camera:\n";
		file << "# CAMERA_ID, MODEL, WIDTH, HEIGHT, PARAMS[]\n";
		file << "# Number of cameras: " << Frames.size() << "\n";

		for (size_t i = 0; i < Frames.size(); ++i)
		{
			float fx = ComputeFocalLengthPixels(Frames[i]);
			float fy = fx;
			if (Frames[i].PixelAspectRatio > 0.0f)
				fy = fx / Frames[i].PixelAspectRatio;

			// center_shift is in the same units as sensor_size (mm); convert to
			// pixel offset on the principal point. See TrackToView.cpp:30 for the
			// canonical centerShift / sensorSize relationship.
			float cx = ImageResolution.x * 0.5f;
			float cy = ImageResolution.y * 0.5f;
			if (Frames[i].SensorSize.x > 0.0f)
				cx += Frames[i].CenterShift.x * ImageResolution.x / Frames[i].SensorSize.x;
			if (Frames[i].SensorSize.y > 0.0f)
				cy += Frames[i].CenterShift.y * ImageResolution.y / Frames[i].SensorSize.y;

			float k1 = Frames[i].K1;
			float k2 = Frames[i].K2;

			file << (i + 1) << " OPENCV " << ImageResolution.x << " " << ImageResolution.y << " "
				 << fx << " " << fy << " " << cx << " " << cy << " "
				 << k1 << " " << k2 << " 0 0\n";
		}
	}

	void WriteImagesTxt(const std::filesystem::path& outDir)
	{
		auto path = outDir / "images.txt";
		std::ofstream file(path);
		if (!file.is_open())
		{
			nosEngine.LogE("RecordTrackCOLMAP: Cannot open %s", nos::PathToUtf8(path).c_str());
			return;
		}

		file << std::setprecision(12);
		file << "# COLMAP poses. Standard format (colmap.github.io/format.html).\n";
		file << "# Frame: RH, +X right, +Y down, +Z forward (camera looks along +Z).\n";
		file << "# (QW, QX, QY, QZ) is the world-to-camera rotation R_w2c.\n";
		file << "# (TX, TY, TZ) is the world-to-camera translation: t = -R_w2c * camera_world_position.\n";
		file << "# Recover camera position in the COLMAP world frame as: C = -R_w2c^T * t.\n";
		file << "# Image list with two lines of data per image:\n";
		file << "# IMAGE_ID, QW, QX, QY, QZ, TX, TY, TZ, CAMERA_ID, NAME\n";
		file << "# POINTS2D[] as (X, Y, POINT3D_ID)\n";
		file << "# Number of images: " << Frames.size() << "\n";

		// M maps the SourceFrame to the COLMAP frame. Used to convert both the
		// source-frame R_c2w and the source-frame camera position into COLMAP.
		// Positions are also scaled into COLMAP's units (meters) by the ratio of
		// the two systems' meters_per_unit.
		const glm::dmat3 M = nos::math::BasisChangeToColmap(SourceFrame);
		const glm::dmat3 Minv = glm::inverse(M);
		const double unitFactor = nos::math::UnitFactor(SourceFrame, nos::math::COLMAP_SYSTEM);

		for (size_t i = 0; i < Frames.size(); ++i)
		{
			auto& frame = Frames[i];

			// Build R_c2w in the source frame, then conjugate by M to land in
			// the COLMAP frame. Likewise frame the position.
			glm::dmat3 R_c2w_src = nos::math::EulerToMat(SourceFrame.euler(), glm::dvec3(frame.Rotation));
			glm::dmat3 R_c2w_colmap = M * R_c2w_src * Minv;
			glm::dvec3 pos_colmap = M * glm::dvec3(frame.Location) * unitFactor;

			glm::dmat3 R_w2c = glm::transpose(R_c2w_colmap);
			glm::dquat q_w2c = glm::quat_cast(R_w2c);
			glm::dvec3 t = -R_w2c * pos_colmap;

			file << (i + 1) << " "
				 << q_w2c.w << " " << q_w2c.x << " " << q_w2c.y << " " << q_w2c.z << " "
				 << t.x << " " << t.y << " " << t.z << " "
				 << (i + 1) << " "
				 << "frame_" << std::setfill('0') << std::setw(6) << i << ".png\n";
			// Empty points line (required by COLMAP format)
			file << "\n";
		}
	}
};

void RegisterRecordTrackCOLMAP(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("RecordTrackCOLMAP"), RecordTrackCOLMAPContext, fn);
}

} // namespace nos::track
