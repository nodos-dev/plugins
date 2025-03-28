// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/PluginHelpers.hpp>

// External
#define STBI_WINDOWS_UTF8
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
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

using Clock = std::chrono::high_resolution_clock;

namespace nos::utilities
{
NOS_REGISTER_NAME_SPACED(Nos_Utilities_StbiLoad, "nos.utilities.StbiLoad")

enum State
{
    Idle = 0,
    Loading = 1,
    Failed = 2,
};

struct StbiLoadContext : NodeContext
{
    decltype(Clock::now()) TimeStarted;

	StbiLoadContext(nosFbNodePtr node) :
		NodeContext(node),
		TimeStarted(Clock::now())
	{
		std::string path;
		bool sRGB = false;
		for (auto* pin : *node->pins())
		{
			auto name = pin->name()->c_str();
			auto data = pin->data();
			if (!data || !data->size())
				continue;
			if (strcmp(name, "Path") == 0)
				path = reinterpret_cast<const char*>(data->data());
			else if (strcmp(name, "sRGB") == 0)
				sRGB = *reinterpret_cast<const bool*>(data->data());
		}
		if (!path.empty())
			LoadImage(nos::Utf8ToPath(path), *GetPinId(NSN_Out), sRGB);
	}

	~StbiLoadContext()
	{
	}

	nosResult ExecuteNode(nosNodeExecuteParams* params) override {
		nos::NodeExecuteParams execParams(params);
		nos::uuid outPinId = execParams[NSN_Out].Id;
		bool sRGB = *InterpretPinValue<bool>(execParams[NSN_sRGB].Data->Data);
		std::filesystem::path FilePath = nos::Utf8ToPath(InterpretPinValue<const char>(execParams[NSN_Path].Data->Data));
		return LoadImage(FilePath, outPinId, sRGB);
	}

	void UpdateStatus(State newState, std::filesystem::path path)
	{
		auto messageDetailsFileRef = std::string("[File](") + NOS_URI_EXPLORER_PREFIX + nos::PathToUtf8(path) + ")";
        switch(newState)
        {
        case State::Loading:
		    TimeStarted = Clock::now();
			SetNodeStatusMessage("Loading image", fb::NodeStatusMessageType::INFO);
            break;
        case State::Idle:
        {
			std::stringstream ss;
			auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - TimeStarted).count();
			std::string fileName = nos::PathToUtf8(path.filename());
			auto messageDetails = messageDetailsFileRef + " is loaded in " + std::to_string(dt) + "ms";
			SetNodeStatusMessages({{{}, "Image loaded", fb::NodeStatusMessageType::INFO, messageDetails, 5, true, true}});
            break;
        }
        case State::Failed:
		{
			auto messageDetails = messageDetailsFileRef + " failed to load";
			SetNodeStatusMessages({{{}, "Failed to load image", fb::NodeStatusMessageType::FAILURE, messageDetails, 10, true, true} });
			break;
		}
        }
	}

	nosResult LoadImage(std::filesystem::path path, nosUUID outPinId, bool sRGB)
	{
		UpdateStatus(State::Loading, path);
		std::string pathUtf8 = nos::PathToUtf8(path);
		try
		{
			int w, h, n;
			uint8_t* img = stbi_load(pathUtf8.c_str(), &w, &h, &n, 4);
			if (!img)
			{
				nosEngine.LogE("Couldn't load image from %s.", path.c_str());
				UpdateStatus(State::Failed, path);
				return NOS_RESULT_FAILED;
			}

			nosResourceShareInfo outResInfo = {
				.Info = {.Type = NOS_RESOURCE_TYPE_TEXTURE,
							.Texture = {.Width = (uint32_t)w, .Height = (uint32_t)h, .Format = NOS_FORMAT_R8G8B8A8_UNORM, .FieldType = NOS_TEXTURE_FIELD_TYPE_PROGRESSIVE}} };

			// unless reading raw bytes, this is useless since samplers convert to linear space automatically
			if (sRGB)
				outResInfo.Info.Texture.Format = NOS_FORMAT_R8G8B8A8_SRGB;

			auto outResOpt = vkss::Resource::Create(outResInfo, "ReadImage Texture");
			if (!outResOpt)
			{
				nosEngine.LogE("Failed to create texture resource for image %s.", path.c_str());
				UpdateStatus(State::Failed, path);
				return NOS_RESULT_FAILED;
			}
			auto outRes = std::move(*outResOpt);

			nosCmd cmd{};
			nosCmdBeginParams beginParams{
			.Name = NOS_NAME("ReadImage Load"),
			.AssociatedNodeId = this->NodeId,
			.OutCmdHandle = &cmd
			};
			nosVulkan->Begin(&beginParams);
			nosVulkan->ImageLoad(cmd, img, nosVec2u(w, h), NOS_FORMAT_R8G8B8A8_SRGB, &outRes, nullptr);
			nosCmdEndParams endParams{ .ForceSubmit = true };
			nosVulkan->End(cmd, &endParams);

			nosEngine.SetPinValue(outPinId, outRes.ToPinData());

			free(img);
			UpdateStatus(State::Idle, path);
		}
		catch (const std::exception& e)
		{
			nosEngine.LogE("Error while loading image: %s", e.what());
			UpdateStatus(State::Failed, path);
		}
		return NOS_RESULT_SUCCESS;
	}

};

nosResult RegisterStbiLoad(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NSN_Nos_Utilities_StbiLoad, StbiLoadContext, fn);
	return NOS_RESULT_SUCCESS;
}

} // namespace nos::utilities