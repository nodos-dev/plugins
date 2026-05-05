// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/PluginHelpers.hpp>
#include "nosSysTrack/Track_generated.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <fstream>
#include <filesystem>
#include <vector>
#include <cmath>
#include <iomanip>

namespace nos::track
{

NOS_REGISTER_NAME(OutputDirectory);
NOS_REGISTER_NAME(ImageResolution);
NOS_REGISTER_NAME(CoordinateSystem);
NOS_REGISTER_NAME(Record);
NOS_REGISTER_NAME(FrameCount);
NOS_REGISTER_NAME(RecordingFrame);

NOS_REGISTER_NAME(RecordTrackCOLMAP_Record);
NOS_REGISTER_NAME(RecordTrackCOLMAP_Stop);
NOS_REGISTER_NAME(RecordTrackCOLMAP_Save);
NOS_REGISTER_NAME(RecordTrackCOLMAP_Clear);
NOS_REGISTER_NAME(RecordTrackCOLMAP_OpenFolder);

struct RecordedFrame
{
	glm::vec3 Location;
	glm::vec3 Rotation; // Euler degrees (roll, tilt, pan)
	float FOV;
	glm::vec2 SensorSize;
	float FocusDistance;
	float PixelAspectRatio;
	float K1;
	float K2;
	std::string Timecode;
	uint32_t FrameNumber;
};

struct RecordTrackCOLMAPContext : NodeContext
{
	std::string OutputDir;
	nosVec2u ImageResolution = {1920, 1080};
	sys::track::CoordinateSystem CoordSys = sys::track::CoordinateSystem::ZYX;
	bool Recording = false;
	bool SyncingRecordPin = false;
	std::string LastError;
	std::vector<RecordedFrame> Frames;
	std::unordered_map<nos::Name, uuid> FunctionIds;

	RecordTrackCOLMAPContext(nosFbNodePtr node) : NodeContext(node)
	{
		if (node->functions())
		{
			for (auto* func : *node->functions())
				FunctionIds[nos::Name(func->class_name()->c_str())] = *func->id();
		}

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
		UpdateFunctionOrphanStates();
		UpdateStatus();
	}

	void SetFunctionOrphanState(nos::Name funcName, fb::NodeOrphanStateType type)
	{
		auto it = FunctionIds.find(funcName);
		if (it != FunctionIds.end())
			NodeContext::SetNodeOrphanState(it->second, type);
	}

	void UpdateFunctionOrphanStates()
	{
		// Disabled: toggling NodeOrphanState on Record/Stop function nodes invalidates the
		// engine's compiled execution path, causing a recompile hitch on the first recorded
		// frame. NodeOrphanStateType has no PASSIVE variant (only pins do), so we can't
		// express "visual-only, no topology change" today. Re-enable once the engine adds
		// PASSIVE to NodeOrphanStateType (or otherwise skips path invalidation for
		// orphan-state changes on function nodes).
	}

	void SyncRecordPin(bool value)
	{
		SyncingRecordPin = true;
		SetPinValue(NSN_Record, nosBuffer{.Data = &value, .Size = sizeof(value)});
		SyncingRecordPin = false;
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
		SyncRecordPin(true);
		UpdateFrameCountPin();
		UpdateRecordingFramePin();
		UpdateFunctionOrphanStates();
		UpdateStatus();
		nosEngine.LogI("RecordTrackCOLMAP: Recording started");
		return true;
	}

	void StopRecording()
	{
		Recording = false;
		SyncRecordPin(false);
		UpdateRecordingFramePin();
		UpdateFunctionOrphanStates();
		nosEngine.LogI("RecordTrackCOLMAP: Recording stopped (%zu frames in buffer)", Frames.size());
		if (!Frames.empty())
			WriteFiles();
		UpdateStatus();
	}

	void OnPinValueChanged(nos::Name pinName, uuid const& pinId, nosBuffer val) override
	{
		if (pinName == NSN_OutputDirectory)
		{
			OutputDir = InterpretPinValue<const char>(val.Data);
			LastError.clear();
			UpdateStatus();
		}
		else if (pinName == NSN_ImageResolution)
			ImageResolution = *(nosVec2u*)val.Data;
		else if (pinName == NSN_CoordinateSystem)
			CoordSys = *(sys::track::CoordinateSystem*)val.Data;
		else if (pinName == NSN_Record)
		{
			if (SyncingRecordPin)
				return;
			bool newVal = *(bool*)val.Data;
			if (newVal && !Recording)
				StartRecording();
			else if (!newVal && Recording)
				StopRecording();
		}
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
		else if (!Frames.empty())
			SetNodeStatusMessage("Idle (" + std::to_string(Frames.size()) + " frames in buffer)", fb::NodeStatusMessageType::INFO);
		else
			SetNodeStatusMessage("Idle", fb::NodeStatusMessageType::INFO);
	}

	nosResult ExecuteNode(nosNodeExecuteParams* params) override
	{
		nos::NodeExecuteParams execParams(params);

		// Pass through Track input to output
		nosBuffer trackBuf{};
		for (size_t i = 0; i < params->PinCount; ++i)
		{
			if (params->Pins[i].Name == NOS_NAME("InTrack"))
			{
				trackBuf = {.Data = (void*)params->Pins[i].Data->Data, .Size = params->Pins[i].Data->Size};
				break;
			}
		}
		SetPinValue(NOS_NAME("OutTrack"), trackBuf);

		if (!Recording)
			return NOS_RESULT_SUCCESS;

		auto* trackData = flatbuffers::GetRoot<sys::track::Track>(trackBuf.Data);
		if (!trackData)
			return NOS_RESULT_SUCCESS;

		RecordedFrame frame{};
		if (const char* tc = execParams.GetPinData<const char>(NOS_NAME_STATIC("Timecode")))
			frame.Timecode = tc;
		frame.FrameNumber = *execParams.GetPinData<uint32_t>(NOS_NAME_STATIC("FrameNumber"));
		if (auto* loc = trackData->location())
			frame.Location = {loc->x(), loc->y(), loc->z()};
		if (auto* rot = trackData->rotation())
			frame.Rotation = {rot->x(), rot->y(), rot->z()};
		frame.FOV = trackData->fov();
		if (auto* ss = trackData->sensor_size())
			frame.SensorSize = {ss->x(), ss->y()};
		frame.FocusDistance = trackData->focus_distance();
		frame.PixelAspectRatio = trackData->pixel_aspect_ratio();
		if (auto* ld = trackData->lens_distortion())
		{
			frame.K1 = ld->k1k2().x();
			frame.K2 = ld->k1k2().y();
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
		nosEngine.LogI("RecordTrackCOLMAP: Saved %zu frames to %s", Frames.size(), OutputDir.c_str());
	}

	void WriteTimecodesTxt(const std::filesystem::path& outDir)
	{
		// Skip the sidecar entirely if no frame carried a timecode — keeps the
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
		file << "# Timecode sidecar paired with images.txt by IMAGE_ID.\n";
		file << "# IMAGE_ID, TIMECODE, FRAME_NUMBER\n";
		file << "# Number of entries: " << Frames.size() << "\n";
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
		file << "# Camera list with one line of data per camera:\n";
		file << "# CAMERA_ID, MODEL, WIDTH, HEIGHT, PARAMS[]\n";
		file << "# Number of cameras: " << Frames.size() << "\n";

		for (size_t i = 0; i < Frames.size(); ++i)
		{
			float fx = ComputeFocalLengthPixels(Frames[i]);
			float fy = fx;
			if (Frames[i].PixelAspectRatio > 0.0f)
				fy = fx / Frames[i].PixelAspectRatio;

			float cx = ImageResolution.x * 0.5f;
			float cy = ImageResolution.y * 0.5f;

			// OPENCV model: fx, fy, cx, cy, k1, k2, p1, p2
			float k1 = Frames[i].K1;
			float k2 = Frames[i].K2;

			file << (i + 1) << " OPENCV " << ImageResolution.x << " " << ImageResolution.y << " "
				 << fx << " " << fy << " " << cx << " " << cy << " "
				 << k1 << " " << k2 << " 0 0\n";
		}
	}

	static glm::mat3 EulerToRotationMatrix(glm::vec3 rot, sys::track::CoordinateSystem order)
	{
		// rot is (roll, tilt, pan) = (x, y, z) in radians
		// Sign convention matches MakeRotation: negate roll (x) and tilt (y)
		float r = -rot.x, t = -rot.y, p = rot.z;
		switch (order)
		{
		default:
		case sys::track::CoordinateSystem::ZYX: return glm::mat3(glm::eulerAngleZYX(p, t, r));
		case sys::track::CoordinateSystem::XYZ: return glm::mat3(glm::eulerAngleXYZ(r, t, p));
		case sys::track::CoordinateSystem::YXZ: return glm::mat3(glm::eulerAngleYXZ(t, r, p));
		case sys::track::CoordinateSystem::YZX: return glm::mat3(glm::eulerAngleYZX(t, p, r));
		case sys::track::CoordinateSystem::ZXY: return glm::mat3(glm::eulerAngleZXY(p, r, t));
		case sys::track::CoordinateSystem::XZY: return glm::mat3(glm::eulerAngleXZY(r, p, t));
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
		file << "# Image list with two lines of data per image:\n";
		file << "# IMAGE_ID, QW, QX, QY, QZ, TX, TY, TZ, CAMERA_ID, NAME\n";
		file << "# POINTS2D[] as (X, Y, POINT3D_ID)\n";
		file << "# Number of images: " << Frames.size() << "\n";

		for (size_t i = 0; i < Frames.size(); ++i)
		{
			auto& frame = Frames[i];

			// Convert Euler angles to rotation matrix
			// Sign convention matches MakeRotation: negate roll (x) and tilt (y)
			glm::vec3 rot = glm::radians(frame.Rotation);
			glm::mat3 R_c2w = EulerToRotationMatrix(rot, CoordSys);

			// COLMAP expects world-to-camera rotation
			glm::mat3 R_w2c = glm::transpose(R_c2w);
			glm::quat q_w2c = glm::quat_cast(R_w2c);

			// COLMAP translation: t = -R * C (camera center in world coords)
			glm::vec3 t = -R_w2c * frame.Location;

			// IMAGE_ID, QW, QX, QY, QZ, TX, TY, TZ, CAMERA_ID, NAME
			file << (i + 1) << " "
				 << q_w2c.w << " " << q_w2c.x << " " << q_w2c.y << " " << q_w2c.z << " "
				 << t.x << " " << t.y << " " << t.z << " "
				 << (i + 1) << " "
				 << "frame_" << std::setfill('0') << std::setw(6) << i << ".png\n";
			// Empty points line (required by COLMAP format)
			file << "\n";
		}
	}

	// TODO: Replace std::system with platform APIs (ShellExecuteW / posix_spawnp) to avoid shell injection via crafted paths
	static void OpenFolderInExplorer(const std::filesystem::path& folder)
	{
#if defined(_WIN32)
		std::string cmd = "explorer \"" + nos::PathToUtf8(folder) + "\"";
#elif defined(__APPLE__)
		std::string cmd = "open \"" + nos::PathToUtf8(folder) + "\"";
#else
		std::string cmd = "xdg-open \"" + nos::PathToUtf8(folder) + "\"";
#endif
		std::system(cmd.c_str());
	}

	static nosResult GetFunctions(size_t* count, nosName* names, nosPfnNodeFunctionExecute* fns)
	{
		*count = 5;
		if (!names || !fns)
			return NOS_RESULT_SUCCESS;

		names[0] = NOS_NAME_STATIC("RecordTrackCOLMAP_Record");
		fns[0] = [](void* ctx, nosFunctionExecuteParams*) {
			auto* self = static_cast<RecordTrackCOLMAPContext*>(ctx);
			if (self->Recording)
				return NOS_RESULT_SUCCESS;
			self->StartRecording();
			return NOS_RESULT_SUCCESS;
		};

		names[1] = NOS_NAME_STATIC("RecordTrackCOLMAP_Stop");
		fns[1] = [](void* ctx, nosFunctionExecuteParams*) {
			auto* self = static_cast<RecordTrackCOLMAPContext*>(ctx);
			if (!self->Recording)
				return NOS_RESULT_SUCCESS;
			self->StopRecording();
			return NOS_RESULT_SUCCESS;
		};

		names[2] = NOS_NAME_STATIC("RecordTrackCOLMAP_Save");
		fns[2] = [](void* ctx, nosFunctionExecuteParams*) {
			auto* self = static_cast<RecordTrackCOLMAPContext*>(ctx);
			self->WriteFiles();
			return NOS_RESULT_SUCCESS;
		};

		names[3] = NOS_NAME_STATIC("RecordTrackCOLMAP_Clear");
		fns[3] = [](void* ctx, nosFunctionExecuteParams*) {
			auto* self = static_cast<RecordTrackCOLMAPContext*>(ctx);
			self->Frames.clear();
			self->UpdateFrameCountPin();
			self->UpdateStatus();
			nosEngine.LogI("RecordTrackCOLMAP: Buffer cleared");
			return NOS_RESULT_SUCCESS;
		};

		names[4] = NOS_NAME_STATIC("RecordTrackCOLMAP_OpenFolder");
		fns[4] = [](void* ctx, nosFunctionExecuteParams*) {
			auto* self = static_cast<RecordTrackCOLMAPContext*>(ctx);
			if (self->OutputDir.empty())
			{
				nosEngine.LogW("RecordTrackCOLMAP: Output directory not set");
				return NOS_RESULT_FAILED;
			}
			std::filesystem::path outDir = nos::Utf8ToPath(self->OutputDir);
			if (!std::filesystem::exists(outDir))
			{
				nosEngine.LogW("RecordTrackCOLMAP: Directory does not exist: %s", self->OutputDir.c_str());
				return NOS_RESULT_FAILED;
			}
			OpenFolderInExplorer(outDir);
			return NOS_RESULT_SUCCESS;
		};

		return NOS_RESULT_SUCCESS;
	}
};

void RegisterRecordTrackCOLMAP(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("RecordTrackCOLMAP"), RecordTrackCOLMAPContext, fn);
}

} // namespace nos::track
