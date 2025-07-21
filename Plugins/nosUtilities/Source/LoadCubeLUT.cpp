// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/PluginHelpers.hpp>

// External
#include <nosCppUtilities.hpp>

// Framework
#include <Builtins_generated.h>
#include <AppService_generated.h>

// nosNodes
#include <nosVulkanSubsystem/Helpers.hpp>

#include "Names.h"

#include <atomic>
#include <chrono>
#include <sstream>
#include <fstream>

using Clock = std::chrono::high_resolution_clock;

namespace nos::utilities
{
NOS_REGISTER_NAME_SPACED(Nos_Utilities_LoadCubeLUT, "nos.utilities.LoadCubeLUT")
NOS_REGISTER_NAME(Internal_ReleaseResource)
enum State
{
    Idle = 0,
    Loading = 1,
    Failed = 2,
};

template<typename T>
struct CubeLUT
{
	using Type = T;
	std::string Title;
	uint32_t LUT3DSize = 0;
	std::array<float, 3> DomainMin = {0.0f, 0.0f, 0.0f};
	std::array<float, 3> DomainMax = {1.0f, 1.0f, 1.0f};
	std::vector<T> LUT3DData;
};

template<typename T>
inline static std::unique_ptr<CubeLUT<T>> LoadCubeFile(const std::filesystem::path& path, bool pushAlpha = true)
{
    CubeLUT<T> file{};
    std::ifstream in(path);
    if (!in.is_open()) {
        return nullptr;
    }

    std::string line;
    size_t dataLines = 0;
	uint32_t componentCount = pushAlpha ? 4 : 3; // RGB or RGBA
    while (std::getline(in, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        if (line.empty() || line[0] == '#')
            continue;
        if (line.find("TITLE", 0) == 0) {
            auto firstQuote = line.find('"');
            auto lastQuote = line.rfind('"');
            if (firstQuote != std::string::npos && lastQuote != std::string::npos && lastQuote > firstQuote)
                file.Title = line.substr(firstQuote + 1, lastQuote - firstQuote - 1);
		}
		else if (line.find("LUT_3D_SIZE", 0) == 0)
		{
            std::istringstream iss(line.substr(12));
            iss >> file.LUT3DSize;
			file.LUT3DData.reserve(file.LUT3DSize * file.LUT3DSize * file.LUT3DSize * componentCount);
		}
		else if (line.find("DOMAIN_MIN", 0) == 0)
		{
            std::istringstream iss(line.substr(10));
			iss >> file.DomainMin[0] >> file.DomainMin[1] >> file.DomainMin[2];
		}
		else if (line.find("DOMAIN_MAX", 0) == 0)
		{
            std::istringstream iss(line.substr(10));
			iss >> file.DomainMax[0] >> file.DomainMax[1] >> file.DomainMax[2];
        } else {
            // Try to parse LUT data
			float r, g, b;
            std::istringstream iss(line);
            if (iss >> r >> g >> b) {
				T rPush = T(std::clamp(r, 0.0f, 1.0f) * std::numeric_limits<T>::max());
				T gPush = T(std::clamp(g, 0.0f, 1.0f) * std::numeric_limits<T>::max());
				T bPush = T(std::clamp(b, 0.0f, 1.0f) * std::numeric_limits<T>::max());
				T aPush = T(std::numeric_limits<T>::max());
				if constexpr(std::is_same_v<T, float>)
				{
					rPush = r;
					gPush = g;
					bPush = b;
					aPush = 1.0f;
				}
				file.LUT3DData.push_back(rPush);
				file.LUT3DData.push_back(gPush);
				file.LUT3DData.push_back(bPush);
				if (pushAlpha)
					file.LUT3DData.push_back(aPush); // Push alpha channel as 1
                ++dataLines;
            }
        }
    }
    // Validate
	size_t expected = file.LUT3DSize * file.LUT3DSize * file.LUT3DSize * componentCount;
    if (file.LUT3DSize == 0 || file.LUT3DData.size() != expected) {
        return nullptr;
    }
    return std::make_unique<CubeLUT<T>>(std::move(file));
}

struct LoadCubeLUTContext : NodeContext
{
    std::atomic<State> CurrentState;
    decltype(Clock::now()) TimeStarted;
	std::filesystem::path FilePath;

	std::mutex OutResDecRefCallbacksMutex;
	std::vector<vkss::Resource> OutPendingResRefs;

	LoadCubeLUTContext(nosFbNodePtr node) : 
		NodeContext(node), 
		CurrentState(State::Idle), 
		TimeStarted(Clock::now())
	{
		std::string path;
		for (auto* pin : *node->pins())
		{
			auto name = pin->name()->c_str();
			auto data = pin->data();
			if (!data || !data->size())
				continue;
			if (strcmp(name, "Path") == 0)
				path = reinterpret_cast<const char*>(data->data());
		}
		if (!path.empty())
			LoadCubeFile(path, *GetPinId(NSN_Out));
	}

	~LoadCubeLUTContext()
	{
		FlushResourceDecRefCallbacks();
	}

	void UpdateStatus(State newState)
	{
        if(newState == CurrentState.exchange(newState))
        {
            return;
        }

        flatbuffers::FlatBufferBuilder fbb;
        std::vector<flatbuffers::Offset<nos::fb::NodeStatusMessage>> msg;
        switch(newState)
        {
        case State::Loading:
		    TimeStarted = Clock::now();
			msg.push_back(fb::CreateNodeStatusMessageDirect(fbb, "Loading LUT", fb::NodeStatusMessageType::INFO));
            break;
        case State::Idle:
        {
			std::stringstream ss;
			auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - TimeStarted).count();
			std::string fileName = nos::PathToUtf8(FilePath.filename());
			auto statusText = std::string("LUT ") + fileName + " loaded in " + std::to_string(dt) + "ms";
			msg.push_back(fb::CreateNodeStatusMessageDirect(fbb, statusText.c_str(), fb::NodeStatusMessageType::INFO));
            break;
        }
        case State::Failed:
            msg.push_back(fb::CreateNodeStatusMessageDirect(fbb, "Failed to load LUT", fb::NodeStatusMessageType::FAILURE));
            break;
        }

        HandleEvent(CreateAppEvent(fbb, nos::CreatePartialNodeUpdateDirect(fbb, &NodeId, ClearFlags::NONE, 0, 0, 0, 0, 0, 0, &msg)));
	}

	void FlushResourceDecRefCallbacks()
	{
		std::lock_guard<std::mutex> lock(OutResDecRefCallbacksMutex);
		OutPendingResRefs.clear();
	}

	nosResult LoadCubeFile(std::filesystem::path path, nosUUID outPinId)
	{
		UpdateStatus(State::Loading);
		FilePath = path;
		std::thread([this, outPinId, path = nos::PathToUtf8(path)]() mutable {
			try
			{
				auto cubeFile = utilities::LoadCubeFile<float>(path);
				if (!cubeFile)
				{
						nosEngine.LogE("Couldn't load cube LUT from %s.", path.c_str());
						UpdateStatus(State::Failed);
						return;
				}

				nosResourceShareInfo outResInfo = {
					.Info = {.Type = NOS_RESOURCE_TYPE_TEXTURE3D,
							 .Texture3D = {.Base{.Width = cubeFile->LUT3DSize,
												 .Height = cubeFile->LUT3DSize,
												 .Format = NOS_FORMAT_R32G32B32A32_SFLOAT,
												 .Filter = NOS_TEXTURE_FILTER_LINEAR,
												 .FieldType = NOS_TEXTURE_FIELD_TYPE_PROGRESSIVE},
										   .Depth = cubeFile->LUT3DSize}}};


				auto outResOpt = vkss::Resource::Create(outResInfo, "Cube LUT");
				if (!outResOpt)
				{
					nosEngine.LogE("Failed to create 3D texture resource for cube LUT %s.", path.c_str());
					UpdateStatus(State::Failed);
					return;
				}
				auto outRes = std::move(*outResOpt);

				nosCmd cmd{};
					nosCmdBeginParams beginParams {
					.Name = NOS_NAME("ReadCubeFile Load"),
					.AssociatedNodeId = this->NodeId, .OutCmdHandle = &cmd};
				nosBufferInfo stagingBufInfo = {
					.Size = (uint32_t)(cubeFile->LUT3DData.size() * sizeof(*cubeFile->LUT3DData.data())),
					.Usage = NOS_BUFFER_USAGE_TRANSFER_SRC,
					.MemoryFlags = NOS_MEMORY_FLAGS_HOST_VISIBLE,
				};
				auto stagingBufOpt = vkss::Resource::Create(stagingBufInfo, "LoadCubeLUT_StagingBuf");
				if (!stagingBufOpt)
				{
					nosEngine.LogE("Failed to create staging buffer for cube LUT file %s.", path.c_str());
					UpdateStatus(State::Failed);
					return;
				}
				auto stagingBuf = std::move(*stagingBufOpt);
				auto stagingBufPtr = nosVulkan->Map(&stagingBuf);
				if (!stagingBufPtr)
				{
					nosEngine.LogE("Failed to map staging buffer for cube LUT file %s.", path.c_str());
					UpdateStatus(State::Failed);
					return;
				}
				std::memcpy(stagingBufPtr,
							cubeFile->LUT3DData.data(),
							cubeFile->LUT3DData.size() * sizeof(*cubeFile->LUT3DData.data()));
				nosVulkan->Begin(&beginParams);
				nosVulkan->Copy(cmd, &stagingBuf, &outRes, "LoadCubeLUT_Copy");
				nosCmdEndParams endParams{ .ForceSubmit = true };
				nosVulkan->End(cmd, &endParams);

				nosEngine.SetPinValue(outPinId, outRes.ToPinData());

				{
					std::lock_guard<std::mutex> lock(this->OutResDecRefCallbacksMutex);
					OutPendingResRefs.push_back(std::move(stagingBuf));
					OutPendingResRefs.push_back(std::move(outRes));
				}
				nosEngine.CallNodeFunction(this->NodeId, NSN_Internal_ReleaseResource);
				nosEngine.TriggerNodeEvent(this->NodeId, NOS_NAME_STATIC("OnCubeLUTLoaded"));

				UpdateStatus(State::Idle);
			}
			catch (const std::exception& e)
			{
				nosEngine.LogE("Error while loading cube file: %s", e.what());
				UpdateStatus(State::Failed);
			}
		}).detach();
		return NOS_RESULT_SUCCESS;
	}

	static nosResult ReleaseResource(void* ctx, nosFunctionExecuteParams* params)
	{
		auto c = (LoadCubeLUTContext*)ctx;
		c->FlushResourceDecRefCallbacks();
		return NOS_RESULT_SUCCESS;
	}
	
	static nosResult Load(void* ctx, nosFunctionExecuteParams* params)
	{
		auto c = (LoadCubeLUTContext*)ctx;
		if (c->CurrentState == State::Loading)
		{
			nosEngine.LogE("Load cube LUT is already loading a file.");
			return NOS_RESULT_FAILED;
		}

		nos::NodeExecuteParams nodeParams(params->ParentNodeExecuteParams);
		std::filesystem::path path = nos::Utf8ToPath(InterpretPinValue<const char>(nodeParams[NSN_Path].Data->Data));
		auto outPinId = nodeParams[NSN_Out].Id;

		return c->LoadCubeFile(path, outPinId);
	}

	static nosResult GetFunctions(size_t* count, nosName* names, nosPfnNodeFunctionExecute* fns)
	{
		*count = 2;
		if (!names || !fns)
			return NOS_RESULT_SUCCESS;

		names[0] = NOS_NAME_STATIC("Load");
		names[1] = NSN_Internal_ReleaseResource;

		fns[0] = &Load;
		fns[1] = &ReleaseResource;

		return NOS_RESULT_SUCCESS;
	}
};

nosResult RegisterLoadCubeLUT(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NSN_Nos_Utilities_LoadCubeLUT, LoadCubeLUTContext, fn);
	return NOS_RESULT_SUCCESS;
}

} // namespace nos::utilities