#include <Nodos/Plugin.hpp>

#include <nosVulkanSubsystem/Helpers.hpp>

namespace nos::utilities
{
struct BoxFitNode : NodeContext
{
	nosResult ExecuteNode(NodeExecuteParams const& params) override
	{
		auto inputTex = params.GetPinObject<sys::vulkan::Texture>(NOS_NAME("Input"));
		auto outputTex = params.GetPinObject<sys::vulkan::Texture>(NOS_NAME("Output"));
		auto inputTexInfo = *sys::vulkan::GetResourceInfo(inputTex);
		auto outputTexInfo = sys::vulkan::GetResourceInfo(outputTex);
		const nos::fb::vec2u& resolution = *params.GetPinData<fb::vec2u>(NOS_NAME("Resolution"));
		
		if (!outputTexInfo || resolution.x() != outputTexInfo->Width || resolution.y() != outputTexInfo->Height)
		{
			// TODO: Transfer output pin should be unscaled
			SetPinObject(NOS_NAME("Output"), sys::vulkan::CreateTexture({
			.Width = resolution.x(), .Height = resolution.y(),
			.Usage =
				nosImageUsage(NOS_IMAGE_USAGE_TRANSFER_DST | NOS_IMAGE_USAGE_TRANSFER_SRC | NOS_IMAGE_USAGE_SAMPLED),
			.FieldType = NOS_TEXTURE_FIELD_TYPE_UNKNOWN
					}, "BoxFitResult"));
		}

		return nosVulkan->ExecuteGPUNode(this, params.RawParams);
	}
};

nosResult RegisterBoxFit(nosNodeFunctions* funcs)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("BoxFit"), BoxFitNode, funcs);
	return NOS_RESULT_SUCCESS;
}
}