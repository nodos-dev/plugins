// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>

// External
#include <nosVulkanSubsystem/Helpers.hpp>

namespace nos::utilities
{

struct Buffer2TextureNodeContext : NodeContext
{
	nosResult ExecuteNode(NodeExecuteParams const& params) override
	{
		auto inBuf = params.GetPinObject<vkss::Buffer>(NOS_NAME("Input"));
		if (!inBuf.IsValid())
		{
			nosEngine.LogE("Buffer2Texture Node: Input buffer is not valid!");
			return NOS_RESULT_FAILED;
		}
		auto outTex = params.GetPinObject<vkss::Texture>(NOS_NAME("Output"));
		const auto& size = *params.GetPinData<fb::vec2u>(NOS_NAME("Size"));
		const auto& format = *params.GetPinData<sys::vulkan::Format>(NOS_NAME("Format"));
		auto inBufInfo = *vkss::GetResourceInfo(inBuf);
		auto outTexInfo = vkss::GetResourceInfo(outTex);
		if (!outTexInfo || size.x() != outTexInfo->Width || size.y() != outTexInfo->Height || format != (nos::sys::vulkan::Format)outTexInfo->Format)
		{
			// Create resource
			outTex = vkss::CreateTexture({.Width = size.x(),
										  .Height = size.y(),
										  .Format = nosFormat(format),
										  .FieldType = inBufInfo.FieldType},
										 "Buffer2Texture Result");
			SetPinObject(NOS_NAME_STATIC("Output"), outTex);
		}

		if (!outTex.IsValid())
			return NOS_RESULT_FAILED;

		nosCmd cmd = vkss::BeginCmd(NOS_NAME("Buffer2Texture Copy"), NodeId);
		nosVulkan->Copy(cmd, inBuf, outTex, 0);
		nosVkGPUEvent event;
		nosCmdEndParams endParams{.ForceSubmit = true, .OutGPUEventHandle = &event};
		nosVulkan->End(cmd, &endParams);
		nosVulkan->WaitGpuEvent(&event, UINT_MAX);

		nosVulkan->SetResourceFieldType(outTex, inBufInfo.FieldType);
		return NOS_RESULT_SUCCESS;
	}
};

nosResult RegisterBuffer2Texture(nosNodeFunctions* funcs)
{
	NOS_BIND_NODE_CLASS(NOS_NAME_STATIC("nos.utilities.Buffer2Texture"), Buffer2TextureNodeContext, funcs);
	return NOS_RESULT_SUCCESS;
}

}