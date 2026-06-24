// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>
#include "nosTrack/Track_generated.h"
#include <nosTrack/PlaybackMode_generated.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <fstream>
#include <filesystem>
#include <vector>
#include <cmath>
#include <cstring>
#include <optional>
#include <sstream>
#include <string>
#include <algorithm>
#include <unordered_map>

#include <nosTrack/CoordinateFrameConversion.hpp>

namespace nos::track
{

NOS_REGISTER_NAME_SPACED(Playback_InputDirectory, "InputDirectory");
NOS_REGISTER_NAME_SPACED(Playback_TargetFrame, "TargetFrame");
NOS_REGISTER_NAME_SPACED(Playback_Mode, "Mode");
NOS_REGISTER_NAME_SPACED(Playback_InFrameIndex, "InFrameIndex");
NOS_REGISTER_NAME_SPACED(Playback_InTimecode, "InTimecode");
NOS_REGISTER_NAME_SPACED(Playback_InFrameNumber, "InFrameNumber");
NOS_REGISTER_NAME_SPACED(Playback_OutFrameIndex, "OutFrameIndex");
NOS_REGISTER_NAME_SPACED(Playback_FrameCount, "FrameCount");

NOS_REGISTER_NAME(PlaybackTrackCOLMAP_OpenFolder);

struct COLMAPCamera
{
	uint32_t Id = 0;
	std::string Model;
	uint32_t Width = 0;
	uint32_t Height = 0;
	float Fx = 0, Fy = 0, Cx = 0, Cy = 0;
	float K1 = 0, K2 = 0, P1 = 0, P2 = 0;
};

struct COLMAPImage
{
	uint32_t Id = 0;
	glm::quat Q{1, 0, 0, 0};  // R_w2c in COLMAP camera frame.
	glm::vec3 T{0};           // t = -R_w2c * camera_world_position (COLMAP world frame).
	uint32_t CameraId = 0;
};

struct TimecodeEntry
{
	std::string Timecode;
	uint32_t FrameNumber = 0;
};

struct ExtrasEntry
{
	bool Present = false;
	float Zoom = 0;
	float Focus = 0;
	float FocusDistance = 0;
	float RenderRatio = 0;
	float NodalOffset = 0;
	float DistortionScale = 0;
	float SensorWmm = 0;
	float SensorHmm = 0;
	float RotX = 0;
	float RotY = 0;
	float RotZ = 0;
};

template <typename TEnum, size_t N>
static bool EnumFromName(TEnum const (&values)[N], char const* const* names, std::string const& name, TEnum& out)
{
	for (size_t i = 0; i < N; ++i)
		if (name == names[i])
		{
			out = values[i];
			return true;
		}
	return false;
}

// Inverse of the "# SourceFrame: up=... forward=... handedness=... euler_order=...
// euler_sign=x,y,z" header written by RecordTrackCOLMAP::WriteExtrasTxt. Returns
// nullopt when any field is missing or unrecognized (e.g. a hand-edited sidecar).
static std::optional<nos::track::Frame> ParseSourceFrameComment(std::string const& line)
{
	auto value = [&](char const* key) -> std::string {
		auto pos = line.find(key);
		if (pos == std::string::npos)
			return {};
		pos += std::strlen(key);
		return line.substr(pos, line.find(' ', pos) - pos);
	};
	nos::track::SignedAxis up{}, fwd{};
	nos::track::Handedness hand{};
	nos::track::EulerOrder order{};
	if (!EnumFromName(nos::track::EnumValuesSignedAxis(), nos::track::EnumNamesSignedAxis(), value("up="), up) ||
		!EnumFromName(nos::track::EnumValuesSignedAxis(), nos::track::EnumNamesSignedAxis(), value("forward="), fwd) ||
		!EnumFromName(nos::track::EnumValuesHandedness(), nos::track::EnumNamesHandedness(), value("handedness="), hand) ||
		!EnumFromName(nos::track::EnumValuesEulerOrder(), nos::track::EnumNamesEulerOrder(), value("euler_order="), order))
		return std::nullopt;

	// euler_sign=x,y,z, each +1/-1. Missing or garbled -> reject the whole header.
	int sx = 0, sy = 0, sz = 0;
	std::string signs = value("euler_sign=");
	std::replace(signs.begin(), signs.end(), ',', ' ');
	std::istringstream ss(signs);
	if (!(ss >> sx >> sy >> sz))
		return std::nullopt;

	// meters_per_unit is irrelevant to rotation, which is all this frame is used for.
	return nos::track::Frame(up, fwd, hand,
		nos::track::EulerEncoding(order, (int8_t)sx, (int8_t)sy, (int8_t)sz), 1.0);
}

// True when Euler angles authored in `a` decode to the same rotation as in `b`
// (same basis and same nested Euler encoding; units don't enter into rotation).
static bool SameRotationConvention(nos::track::Frame const& a, nos::track::Frame const& b)
{
	return a.up() == b.up() && a.forward() == b.forward() && a.handedness() == b.handedness() &&
		   a.euler().order() == b.euler().order() &&
		   a.euler().sign_x() == b.euler().sign_x() &&
		   a.euler().sign_y() == b.euler().sign_y() &&
		   a.euler().sign_z() == b.euler().sign_z();
}

struct PlaybackTrackCOLMAPContext : NodeContext
{
	std::string InputDir;
	nos::track::Frame TargetFrame = nos::track::UNREAL_SYSTEM;
	PlaybackTrackMode Mode = PlaybackTrackMode::FrameIndex;
	uint32_t FrameIndex = 0;
	std::string InTimecode;
	uint32_t InFrameNumber = 0;
	std::string LastError;
	std::vector<nos::track::TTrack> Frames;
	std::vector<TimecodeEntry> Timecodes; // empty or same size as Frames
	std::unordered_map<std::string, uint32_t> TimecodeToIndex;
	std::unordered_map<uint32_t, uint32_t> FrameNumberToIndex;
	uint32_t CurrentFrame = 0;

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
		ApplyModeOrphanStates();
		UpdateStatus();
		return NOS_RESULT_SUCCESS;
	}

	void OnPinValueChanged(nos::Name pinName, uuid const& pinId, nosBuffer val) override
	{
		if (pinName == NSN_Playback_InputDirectory)
		{
			InputDir = static_cast<const char*>(val.Data);
			LastError.clear();
			if (!InputDir.empty())
				LoadFromDirectory();
			else
				UpdateStatus();
		}
		else if (pinName == NSN_Playback_TargetFrame)
		{
			TargetFrame = *(nos::track::Frame*)val.Data;
			if (!InputDir.empty())
				LoadFromDirectory();
		}
		else if (pinName == NSN_Playback_Mode)
		{
			Mode = *(PlaybackTrackMode*)val.Data;
			ApplyModeOrphanStates();
		}
		else if (pinName == NSN_Playback_InFrameIndex)
			FrameIndex = *(uint32_t*)val.Data;
		else if (pinName == NSN_Playback_InTimecode)
			InTimecode = static_cast<const char*>(val.Data);
		else if (pinName == NSN_Playback_InFrameNumber)
			InFrameNumber = *(uint32_t*)val.Data;
	}

	void ApplyModeOrphanStates()
	{
		auto state = [](bool active) {
			return active ? fb::PinOrphanStateType::ACTIVE : fb::PinOrphanStateType::PASSIVE;
		};
		const bool useIdx = Mode == PlaybackTrackMode::FrameIndex;
		const bool useTC  = Mode == PlaybackTrackMode::Timecode;
		const bool useFN  = Mode == PlaybackTrackMode::FrameNumber;
		SetPinOrphanState(NSN_Playback_InFrameIndex,  state(useIdx));
		SetPinOrphanState(NSN_Playback_InTimecode,    state(useTC));
		SetPinOrphanState(NSN_Playback_InFrameNumber, state(useFN));
	}

	void UpdateFrameCountPin()
	{
		uint32_t count = (uint32_t)Frames.size();
		SetPinValue(NSN_Playback_FrameCount, nosBuffer{.Data = &count, .Size = sizeof(count)});
	}

	void UpdateFrameIndexPin()
	{
		SetPinValue(NSN_Playback_OutFrameIndex, nosBuffer{.Data = &CurrentFrame, .Size = sizeof(CurrentFrame)});
	}

	void UpdateStatus()
	{
		if (!LastError.empty())
			SetNodeStatusMessage(LastError, fb::NodeStatusMessageType::FAILURE);
		else if (InputDir.empty())
			SetNodeStatusMessage("Set input directory", fb::NodeStatusMessageType::WARNING);
		else if (Frames.empty())
			SetNodeStatusMessage("No data loaded", fb::NodeStatusMessageType::WARNING);
		else
			SetNodeStatusMessage("Loaded (" + std::to_string(Frames.size()) + " frames)", fb::NodeStatusMessageType::INFO);
	}

	// --- Parsing ---

	bool LoadFromDirectory()
	{
		if (InputDir.empty())
		{
			LastError = "Set input directory";
			UpdateStatus();
			return false;
		}

		std::filesystem::path dir = nos::Utf8ToPath(InputDir);
		auto camerasPath = dir / "cameras.txt";
		auto imagesPath = dir / "images.txt";

		if (!std::filesystem::exists(camerasPath))
		{
			LastError = "cameras.txt not found";
			UpdateStatus();
			return false;
		}
		if (!std::filesystem::exists(imagesPath))
		{
			LastError = "images.txt not found";
			UpdateStatus();
			return false;
		}

		std::unordered_map<uint32_t, COLMAPCamera> cameras;
		if (!ParseCamerasTxt(camerasPath, cameras))
			return false;

		std::vector<COLMAPImage> images;
		if (!ParseImagesTxt(imagesPath, images))
			return false;

		if (images.empty())
		{
			LastError = "No images found in images.txt";
			UpdateStatus();
			return false;
		}

		Frames.clear();
		Frames.reserve(images.size());
		Timecodes.clear();

		auto timecodesPath = dir / "timecodes.txt";
		if (std::filesystem::exists(timecodesPath))
			ParseTimecodesTxt(timecodesPath, images.size());

		std::vector<ExtrasEntry> extras;
		std::optional<nos::track::Frame> extrasFrame;
		auto extrasPath = dir / "extras.txt";
		if (std::filesystem::exists(extrasPath))
			ParseExtrasTxt(extrasPath, images.size(), extras, extrasFrame);

		// Inverse of RecordTrackCOLMAP::WriteImagesTxt:
		//   images.txt holds R_w2c in COLMAP frame, t = -R_w2c * pos_colmap.
		//   pos_colmap = -R_c2w_colmap * t   (R_c2w_colmap = R_w2c^T)
		//   pos_target = M^-1 * pos_colmap
		//   R_c2w_target = M^-1 * R_c2w_colmap * M
		//   Track.rotation = MatToEuler(TargetFrame, R_c2w_target)
		const glm::dmat3 Minv = nos::track::BasisChangeFromColmap(TargetFrame);
		const glm::dmat3 M = glm::inverse(Minv);
		// Scale COLMAP units (meters) back into the target system's units.
		const double unitFactor = nos::track::UnitFactor(nos::track::COLMAP_SYSTEM, TargetFrame);

		// Extras Euler is authored in the recording SourceFrame (recorded in the
		// sidecar header). Re-express it in TargetFrame when the conventions
		// differ; a sidecar without a parseable header is assumed to already
		// match TargetFrame (pre-header recordings).
		const bool convertExtrasRotation = extrasFrame && !SameRotationConvention(*extrasFrame, TargetFrame);
		glm::dmat3 extrasC(1.0);
		if (convertExtrasRotation)
			extrasC = nos::track::BasisMatrix(TargetFrame) * glm::inverse(nos::track::BasisMatrix(*extrasFrame));

		for (size_t i = 0; i < images.size(); ++i)
		{
			auto& img = images[i];
			nos::track::TTrack trackData{};
			auto camIt = cameras.find(img.CameraId);
			const ExtrasEntry* ex = (i < extras.size() && extras[i].Present) ? &extras[i] : nullptr;

			glm::dmat3 R_w2c = glm::dmat3(glm::mat3_cast(img.Q));
			glm::dmat3 R_c2w_colmap = glm::transpose(R_w2c);
			glm::dvec3 pos_colmap = -R_c2w_colmap * glm::dvec3(img.T);

			glm::dvec3 pos_target = Minv * pos_colmap * unitFactor;
			glm::vec3 locF((float)pos_target.x, (float)pos_target.y, (float)pos_target.z);
			trackData.location = reinterpret_cast<nos::fb::vec3&>(locF);

			// Rotation: prefer the original Euler from extras (avoids quaternion-
			// to-Euler ambiguity near gimbal lock), converted into TargetFrame if
			// it was recorded in a different one; fall back to extracting from
			// the COLMAP rotation matrix when no extras sidecar exists.
			if (ex)
			{
				glm::vec3 euler(ex->RotX, ex->RotY, ex->RotZ);
				if (convertExtrasRotation)
				{
					glm::dmat3 R_src = nos::track::EulerToMat(extrasFrame->euler(), glm::dvec3(euler));
					euler = glm::vec3(nos::track::MatToEuler(TargetFrame.euler(), extrasC * R_src * glm::transpose(extrasC)));
				}
				trackData.rotation = reinterpret_cast<nos::fb::vec3&>(euler);
			}
			else
			{
				glm::dmat3 R_c2w_target = Minv * R_c2w_colmap * M;
				glm::dvec3 eulerD = nos::track::MatToEuler(TargetFrame.euler(), R_c2w_target);
				glm::vec3 eulerF((float)eulerD.x, (float)eulerD.y, (float)eulerD.z);
				trackData.rotation = reinterpret_cast<nos::fb::vec3&>(eulerF);
			}

			if (camIt != cameras.end())
			{
				auto& cam = camIt->second;
				if (cam.Fx > 0)
					trackData.fov = glm::degrees(2.0f * std::atan(cam.Width * 0.5f / cam.Fx));
				if (cam.Fx > 0 && cam.Fy > 0)
					trackData.pixel_aspect_ratio = cam.Fx / cam.Fy;
				trackData.lens_distortion.mutable_k1k2() = nos::fb::vec2(cam.K1, cam.K2);

				// sensor_size: COLMAP only stores pixel dims, but Track expects mm.
				// Use the extras value when present; otherwise fall back to pixels
				// (matches pre-extras behaviour).
				glm::vec2 sensorMm(0);
				if (ex && ex->SensorWmm > 0 && ex->SensorHmm > 0)
				{
					sensorMm = {ex->SensorWmm, ex->SensorHmm};
					trackData.sensor_size = nos::fb::vec2(sensorMm.x, sensorMm.y);
				}
				else
				{
					trackData.sensor_size = nos::fb::vec2(cam.Width, cam.Height);
				}

				// center_shift: invert the (cx, cy) encoding written by record.
				// Needs sensor_size in mm to be meaningful, so only reconstructed
				// when extras provided it.
				if (sensorMm.x > 0 && cam.Width > 0 && sensorMm.y > 0 && cam.Height > 0)
				{
					glm::vec2 shift{
						(cam.Cx - cam.Width * 0.5f) * sensorMm.x / cam.Width,
						(cam.Cy - cam.Height * 0.5f) * sensorMm.y / cam.Height};
					trackData.lens_distortion.mutable_center_shift() = reinterpret_cast<nos::fb::vec2&>(shift);
				}
			}

			if (ex)
			{
				trackData.zoom = ex->Zoom;
				trackData.focus = ex->Focus;
				trackData.focus_distance = ex->FocusDistance;
				trackData.render_ratio = ex->RenderRatio;
				trackData.nodal_offset = ex->NodalOffset;
				trackData.lens_distortion.mutate_distortion_scale(ex->DistortionScale);
			}

			Frames.push_back(std::move(trackData));
		}

		CurrentFrame = 0;
		LastError.clear();
		UpdateFrameCountPin();
		UpdateFrameIndexPin();
		UpdateStatus();
		nosEngine.LogI("PlaybackTrackCOLMAP: Loaded %zu frames from %s", Frames.size(), InputDir.c_str());
		return true;
	}

	bool ParseCamerasTxt(const std::filesystem::path& path, std::unordered_map<uint32_t, COLMAPCamera>& cameras)
	{
		std::ifstream file(path);
		if (!file.is_open())
		{
			LastError = "Cannot open cameras.txt";
			UpdateStatus();
			return false;
		}

		std::string line;
		while (std::getline(file, line))
		{
			if (line.empty() || line[0] == '#')
				continue;
			std::istringstream ss(line);
			COLMAPCamera cam;
			ss >> cam.Id >> cam.Model >> cam.Width >> cam.Height;
			if (cam.Model == "OPENCV")
				ss >> cam.Fx >> cam.Fy >> cam.Cx >> cam.Cy >> cam.K1 >> cam.K2 >> cam.P1 >> cam.P2;
			else if (cam.Model == "PINHOLE")
				ss >> cam.Fx >> cam.Fy >> cam.Cx >> cam.Cy;
			else if (cam.Model == "SIMPLE_PINHOLE")
			{
				float f;
				ss >> f >> cam.Cx >> cam.Cy;
				cam.Fx = cam.Fy = f;
			}
			else if (cam.Model == "SIMPLE_RADIAL")
			{
				float f;
				ss >> f >> cam.Cx >> cam.Cy >> cam.K1;
				cam.Fx = cam.Fy = f;
			}
			else if (cam.Model == "RADIAL")
			{
				float f;
				ss >> f >> cam.Cx >> cam.Cy >> cam.K1 >> cam.K2;
				cam.Fx = cam.Fy = f;
			}
			else
			{
				nosEngine.LogW("PlaybackTrackCOLMAP: Unsupported camera model '%s', treating as PINHOLE", cam.Model.c_str());
				ss >> cam.Fx >> cam.Fy >> cam.Cx >> cam.Cy;
			}
			cameras[cam.Id] = cam;
		}
		return true;
	}

	bool ParseImagesTxt(const std::filesystem::path& path, std::vector<COLMAPImage>& images)
	{
		std::ifstream file(path);
		if (!file.is_open())
		{
			LastError = "Cannot open images.txt";
			UpdateStatus();
			return false;
		}

		std::string line;
		while (std::getline(file, line))
		{
			if (line.empty() || line[0] == '#')
				continue;
			std::istringstream ss(line);
			COLMAPImage img;
			float qw, qx, qy, qz;
			std::string name;
			ss >> img.Id >> qw >> qx >> qy >> qz
			   >> img.T.x >> img.T.y >> img.T.z
			   >> img.CameraId >> name;
			img.Q = glm::quat(qw, qx, qy, qz);
			images.push_back(img);
			// Skip POINTS2D line
			std::getline(file, line);
		}

		std::sort(images.begin(), images.end(), [](auto& a, auto& b) { return a.Id < b.Id; });
		return true;
	}

	void ParseExtrasTxt(const std::filesystem::path& path, size_t expectedCount, std::vector<ExtrasEntry>& outExtras,
						std::optional<nos::track::Frame>& outSourceFrame)
	{
		std::ifstream file(path);
		if (!file.is_open())
			return;
		std::unordered_map<uint32_t, ExtrasEntry> byId;
		std::string line;
		while (std::getline(file, line))
		{
			if (line.rfind("# SourceFrame:", 0) == 0)
			{
				outSourceFrame = ParseSourceFrameComment(line);
				continue;
			}
			if (line.empty() || line[0] == '#')
				continue;
			std::istringstream ss(line);
			uint32_t id = 0;
			ExtrasEntry e;
			ss >> id >> e.Zoom >> e.Focus >> e.FocusDistance >> e.RenderRatio
			   >> e.NodalOffset >> e.DistortionScale
			   >> e.SensorWmm >> e.SensorHmm
			   >> e.RotX >> e.RotY >> e.RotZ;
			if (!ss.fail())
			{
				e.Present = true;
				byId[id] = e;
			}
		}
		outExtras.assign(expectedCount, ExtrasEntry{});
		for (size_t i = 0; i < expectedCount; ++i)
		{
			auto it = byId.find(uint32_t(i + 1));
			if (it != byId.end())
				outExtras[i] = it->second;
		}
	}

	void ParseTimecodesTxt(const std::filesystem::path& path, size_t expectedCount)
	{
		std::ifstream file(path);
		if (!file.is_open())
			return;
		std::unordered_map<uint32_t, TimecodeEntry> byId;
		std::string line;
		while (std::getline(file, line))
		{
			if (line.empty() || line[0] == '#')
				continue;
			std::istringstream ss(line);
			uint32_t id = 0;
			TimecodeEntry e;
			ss >> id >> e.Timecode >> e.FrameNumber;
			if (e.Timecode == "-")
				e.Timecode.clear();
			byId[id] = std::move(e);
		}
		Timecodes.assign(expectedCount, TimecodeEntry{});
		TimecodeToIndex.clear();
		FrameNumberToIndex.clear();
		for (size_t i = 0; i < expectedCount; ++i)
		{
			auto it = byId.find(uint32_t(i + 1));
			if (it == byId.end())
				continue;
			Timecodes[i] = it->second;
			if (!Timecodes[i].Timecode.empty())
				TimecodeToIndex.emplace(Timecodes[i].Timecode, uint32_t(i));
			FrameNumberToIndex.emplace(Timecodes[i].FrameNumber, uint32_t(i));
		}
	}

	// --- Execution ---

	bool ResolveFrameIndex(uint32_t& outIdx)
	{
		switch (Mode)
		{
		case PlaybackTrackMode::Timecode:
		{
			auto it = TimecodeToIndex.find(InTimecode);
			if (it == TimecodeToIndex.end())
				return false;
			outIdx = it->second;
			return true;
		}
		case PlaybackTrackMode::FrameNumber:
		{
			auto it = FrameNumberToIndex.find(InFrameNumber);
			if (it == FrameNumberToIndex.end())
				return false;
			outIdx = it->second;
			return true;
		}
		case PlaybackTrackMode::FrameIndex:
		default:
			outIdx = FrameIndex < (uint32_t)Frames.size() ? FrameIndex : (uint32_t)Frames.size() - 1;
			return true;
		}
	}

	nosResult ExecuteNode(nos::NodeExecuteParams const& params) override
	{
		if (Frames.empty())
		{
			nos::track::TTrack empty{};
			auto buf = nos::Buffer::From(empty);
			SetPinValue(NOS_NAME("Track"), {.Data = buf.Data(), .Size = buf.Size()});
			return NOS_RESULT_SUCCESS;
		}

		uint32_t frameIdx = 0;
		if (!ResolveFrameIndex(frameIdx))
			frameIdx = CurrentFrame < (uint32_t)Frames.size() ? CurrentFrame : 0;
		CurrentFrame = frameIdx;

		auto buf = nos::Buffer::From(Frames[frameIdx]);
		SetPinValue(NOS_NAME("Track"), {.Data = buf.Data(), .Size = buf.Size()});
		UpdateFrameIndexPin();

		return NOS_RESULT_SUCCESS;
	}

	static nosResult GetFunctions(size_t* count, nosName* names, nosPfnNodeFunctionExecute* fns)
	{
		*count = 1;
		if (!names || !fns)
			return NOS_RESULT_SUCCESS;

		names[0] = NOS_NAME_STATIC("PlaybackTrackCOLMAP_OpenFolder");
		fns[0] = [](void* ctx, nosFunctionExecuteParams*) {
			auto* self = static_cast<PlaybackTrackCOLMAPContext*>(ctx);
			if (self->InputDir.empty())
			{
				nosEngine.LogW("PlaybackTrackCOLMAP: Input directory not set");
				return NOS_RESULT_FAILED;
			}
			std::filesystem::path dir = nos::Utf8ToPath(self->InputDir);
			if (!std::filesystem::exists(dir))
			{
				nosEngine.LogW("PlaybackTrackCOLMAP: Directory does not exist: %s", self->InputDir.c_str());
				return NOS_RESULT_FAILED;
			}
			// TODO: Replace std::system with platform APIs (ShellExecuteW / posix_spawnp)
#if defined(_WIN32)
			std::string cmd = "explorer \"" + nos::PathToUtf8(dir) + "\"";
#elif defined(__APPLE__)
			std::string cmd = "open \"" + nos::PathToUtf8(dir) + "\"";
#else
			std::string cmd = "xdg-open \"" + nos::PathToUtf8(dir) + "\"";
#endif
			std::system(cmd.c_str());
			return NOS_RESULT_SUCCESS;
		};

		return NOS_RESULT_SUCCESS;
	}
};

void RegisterPlaybackTrackCOLMAP(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("PlaybackTrackCOLMAP"), PlaybackTrackCOLMAPContext, fn);
}

} // namespace nos::track
