// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/PluginHelpers.hpp>
#include "Track_generated.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <fstream>
#include <filesystem>
#include <vector>
#include <cmath>
#include <sstream>
#include <string>
#include <algorithm>

namespace nos::track
{

NOS_REGISTER_NAME_SPACED(Playback_InputDirectory, "InputDirectory");
NOS_REGISTER_NAME_SPACED(Playback_EulerOrder, "EulerOrder");
NOS_REGISTER_NAME_SPACED(Playback_Mode, "Mode");
NOS_REGISTER_NAME_SPACED(Playback_Loop, "Loop");
NOS_REGISTER_NAME_SPACED(Playback_InFrameIndex, "InFrameIndex");
NOS_REGISTER_NAME_SPACED(Playback_OutFrameIndex, "OutFrameIndex");
NOS_REGISTER_NAME_SPACED(Playback_FrameCount, "FrameCount");

NOS_REGISTER_NAME(PlaybackTrackCOLMAP_Play);
NOS_REGISTER_NAME(PlaybackTrackCOLMAP_Stop);
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
	glm::quat Q{1, 0, 0, 0};
	glm::vec3 T{0};
	uint32_t CameraId = 0;
};

struct PlaybackTrackCOLMAPContext : NodeContext
{
	std::string InputDir;
	track::EulerOrder EulerOrd = track::EulerOrder::ZYX;
	track::PlaybackMode Mode = track::PlaybackMode::Sequential;
	bool Loop = true;
	bool Playing = false;
	uint32_t ManualFrame = 0;
	std::string LastError;
	std::vector<track::TTrack> Frames;
	uint32_t CurrentFrame = 0;
	std::unordered_map<nos::Name, uuid> FunctionIds;
	std::unordered_map<nos::Name, uuid> PinIds;

	PlaybackTrackCOLMAPContext(nosFbNodePtr node) : NodeContext(node)
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
				PinIds[name] = *pin->id();
				if (flatbuffers::IsFieldPresent(pin, fb::Pin::VT_DATA))
				{
					nosBuffer value = {.Data = (void*)pin->data()->data(), .Size = pin->data()->size()};
					OnPinValueChanged(name, *pin->id(), value);
				}
			}
		}
		UpdateOrphanStates();
		UpdateStatus();
	}

	void SetFunctionOrphanState(nos::Name funcName, fb::NodeOrphanStateType type)
	{
		auto it = FunctionIds.find(funcName);
		if (it != FunctionIds.end())
			NodeContext::SetNodeOrphanState(it->second, type);
	}

	void SetPinOrphanState(nos::Name pinName, fb::PinOrphanStateType type)
	{
		auto it = PinIds.find(pinName);
		if (it != PinIds.end())
			NodeContext::SetPinOrphanState(it->second, type);
	}

	void UpdateOrphanStates()
	{
		bool sequential = Mode == track::PlaybackMode::Sequential;

		// Sequential: Play/Stop active, Load/FrameInput orphaned
		// Manual: Load/FrameInput active, Play/Stop orphaned
		if (sequential)
		{
			SetPinOrphanState(NSN_Playback_InFrameIndex, fb::PinOrphanStateType::ORPHAN);

			if (Playing)
			{
				SetFunctionOrphanState(NSN_PlaybackTrackCOLMAP_Play, fb::NodeOrphanStateType::ORPHAN);
				SetFunctionOrphanState(NSN_PlaybackTrackCOLMAP_Stop, fb::NodeOrphanStateType::ACTIVE);
			}
			else
			{
				SetFunctionOrphanState(NSN_PlaybackTrackCOLMAP_Play, fb::NodeOrphanStateType::ACTIVE);
				SetFunctionOrphanState(NSN_PlaybackTrackCOLMAP_Stop, fb::NodeOrphanStateType::ORPHAN);
			}
		}
		else
		{
			SetFunctionOrphanState(NSN_PlaybackTrackCOLMAP_Play, fb::NodeOrphanStateType::ORPHAN);
			SetFunctionOrphanState(NSN_PlaybackTrackCOLMAP_Stop, fb::NodeOrphanStateType::ORPHAN);
			SetPinOrphanState(NSN_Playback_InFrameIndex, fb::PinOrphanStateType::ACTIVE);
		}
	}

	void OnPinValueChanged(nos::Name pinName, uuid const& pinId, nosBuffer val) override
	{
		if (pinName == NSN_Playback_InputDirectory)
		{
			InputDir = InterpretPinValue<const char>(val.Data);
			LastError.clear();
			if (Mode == track::PlaybackMode::Manual && !InputDir.empty())
				LoadFromDirectory();
			else
				UpdateStatus();
		}
		else if (pinName == NSN_Playback_EulerOrder)
			EulerOrd = *(track::EulerOrder*)val.Data;
		else if (pinName == NSN_Playback_Mode)
		{
			auto newMode = *(track::PlaybackMode*)val.Data;
			if (newMode != Mode)
			{
				Mode = newMode;
				Playing = false;
				UpdateOrphanStates();
				UpdateStatus();
			}
		}
		else if (pinName == NSN_Playback_Loop)
			Loop = *(bool*)val.Data;
		else if (pinName == NSN_Playback_InFrameIndex)
			ManualFrame = *(uint32_t*)val.Data;
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
		else if (Mode == track::PlaybackMode::Sequential && Playing)
			SetNodeStatusMessage("Playing (" + std::to_string(CurrentFrame + 1) + "/" + std::to_string(Frames.size()) + ")", fb::NodeStatusMessageType::INFO);
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

		for (auto& img : images)
		{
			track::TTrack trackData{};
			auto camIt = cameras.find(img.CameraId);

			// Convert COLMAP world-to-camera back to camera-to-world
			glm::mat3 R_w2c = glm::mat3_cast(img.Q);
			glm::mat3 R_c2w = glm::transpose(R_w2c);
			glm::vec3 C = -R_c2w * img.T;

			glm::vec3 euler = RotationMatrixToEuler(R_c2w, EulerOrd);
			trackData.location = reinterpret_cast<nos::fb::vec3&>(C);
			trackData.rotation = reinterpret_cast<nos::fb::vec3&>(euler);

			if (camIt != cameras.end())
			{
				auto& cam = camIt->second;
				if (cam.Fx > 0)
					trackData.fov = glm::degrees(2.0f * std::atan(cam.Width * 0.5f / cam.Fx));
				trackData.sensor_size = nos::fb::vec2(cam.Width, cam.Height);
				if (cam.Fx > 0 && cam.Fy > 0)
					trackData.pixel_aspect_ratio = cam.Fx / cam.Fy;
				trackData.lens_distortion.mutable_k1k2() = nos::fb::vec2(cam.K1, cam.K2);
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

	// --- Euler extraction (inverse of EulerToRotationMatrix in RecordTrackCOLMAP) ---

	static glm::vec3 RotationMatrixToEuler(const glm::mat3& R_c2w, track::EulerOrder order)
	{
		float r, t, p;
		switch (order)
		{
		default:
		case track::EulerOrder::ZYX: glm::extractEulerAngleZYX(glm::mat4(R_c2w), p, t, r); break;
		case track::EulerOrder::XYZ: glm::extractEulerAngleXYZ(glm::mat4(R_c2w), r, t, p); break;
		case track::EulerOrder::YXZ: glm::extractEulerAngleYXZ(glm::mat4(R_c2w), t, r, p); break;
		case track::EulerOrder::YZX: glm::extractEulerAngleYZX(glm::mat4(R_c2w), t, p, r); break;
		case track::EulerOrder::ZXY: glm::extractEulerAngleZXY(glm::mat4(R_c2w), p, r, t); break;
		case track::EulerOrder::XZY: glm::extractEulerAngleXZY(glm::mat4(R_c2w), r, p, t); break;
		}
		// Undo sign convention: r = -roll, t = -tilt, p = pan
		return glm::degrees(glm::vec3(-r, -t, p));
	}

	// --- Execution ---

	nosResult ExecuteNode(nosNodeExecuteParams* params) override
	{
		if (Frames.empty())
		{
			track::TTrack empty{};
			auto buf = nos::Buffer::From(empty);
			SetPinValue(NOS_NAME("Track"), {.Data = buf.Data(), .Size = buf.Size()});
			return NOS_RESULT_SUCCESS;
		}

		uint32_t frameIdx = 0;
		if (Mode == track::PlaybackMode::Sequential)
		{
			if (!Playing)
			{
				frameIdx = CurrentFrame;
			}
			else
			{
				frameIdx = CurrentFrame;
				uint32_t next = CurrentFrame + 1;
				if (next >= (uint32_t)Frames.size())
					next = Loop ? 0 : (uint32_t)Frames.size() - 1;
				CurrentFrame = next;
			}
		}
		else
		{
			frameIdx = ManualFrame < (uint32_t)Frames.size() ? ManualFrame : (uint32_t)Frames.size() - 1;
			CurrentFrame = frameIdx;
		}

		auto buf = nos::Buffer::From(Frames[frameIdx]);
		SetPinValue(NOS_NAME("Track"), {.Data = buf.Data(), .Size = buf.Size()});
		UpdateFrameIndexPin();

		if (Mode == track::PlaybackMode::Sequential && Playing)
			UpdateStatus();

		return NOS_RESULT_SUCCESS;
	}

	static nosResult GetFunctions(size_t* count, nosName* names, nosPfnNodeFunctionExecute* fns)
	{
		*count = 3;
		if (!names || !fns)
			return NOS_RESULT_SUCCESS;

		names[0] = NOS_NAME_STATIC("PlaybackTrackCOLMAP_Play");
		fns[0] = [](void* ctx, nosFunctionExecuteParams*) {
			auto* self = static_cast<PlaybackTrackCOLMAPContext*>(ctx);
			if (self->Playing)
				return NOS_RESULT_SUCCESS;
			if (self->Frames.empty())
				self->LoadFromDirectory();
			if (self->Frames.empty())
				return NOS_RESULT_SUCCESS;
			self->Playing = true;
			self->CurrentFrame = 0;
			self->UpdateOrphanStates();
			self->UpdateStatus();
			nosEngine.LogI("PlaybackTrackCOLMAP: Playing (%zu frames)", self->Frames.size());
			return NOS_RESULT_SUCCESS;
		};

		names[1] = NOS_NAME_STATIC("PlaybackTrackCOLMAP_Stop");
		fns[1] = [](void* ctx, nosFunctionExecuteParams*) {
			auto* self = static_cast<PlaybackTrackCOLMAPContext*>(ctx);
			if (!self->Playing)
				return NOS_RESULT_SUCCESS;
			self->Playing = false;
			self->UpdateOrphanStates();
			self->UpdateStatus();
			nosEngine.LogI("PlaybackTrackCOLMAP: Stopped at frame %u", self->CurrentFrame);
			return NOS_RESULT_SUCCESS;
		};

		names[2] = NOS_NAME_STATIC("PlaybackTrackCOLMAP_OpenFolder");
		fns[3] = [](void* ctx, nosFunctionExecuteParams*) {
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
