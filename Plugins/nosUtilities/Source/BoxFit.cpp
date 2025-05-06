#include <Nodos/PluginHelpers.hpp>

#include <nosVulkanSubsystem/Helpers.hpp>

namespace nos::utilities
{
struct BoxFitNode : NodeContext
{
	nosResult ExecuteNode(nosNodeExecuteParams* params) override
	{
		nos::NodeExecuteParams execParams(params);
		const nosBuffer* inputPinData = execParams[NOS_NAME("Input")].Data;
		const nosBuffer* outputPinData = execParams[NOS_NAME("Output")].Data;
		const nos::fb::vec2u& resolution = *execParams.GetPinData<fb::vec2u>(NOS_NAME("Resolution"));
		auto input = vkss::DeserializeTextureInfo(inputPinData->Data);
		auto& output = *InterpretPinValue<sys::vulkan::Texture>(outputPinData->Data);
		
		if (resolution.x() != output.width() || resolution.y() != output.height())
		{
			nosResourceShareInfo bufInfo = {
				.Info = {
					.Type = NOS_RESOURCE_TYPE_TEXTURE,
					.Texture = nosTextureInfo{
						.Width = resolution.x(),
						.Height = resolution.y(),
						.Usage = nosImageUsage(NOS_IMAGE_USAGE_TRANSFER_DST | NOS_IMAGE_USAGE_TRANSFER_SRC | NOS_IMAGE_USAGE_SAMPLED),
						.FieldType = NOS_TEXTURE_FIELD_TYPE_UNKNOWN
					}}};
			auto bufferDesc = vkss::ConvertBufferInfo(bufInfo);
			nosEngine.SetPinValueByName(NodeId, NOS_NAME_STATIC("Output"), Buffer::From(bufferDesc));
		}

		return nosVulkan->ExecuteGPUNode(this, params);
	}
};

nosResult RegisterBoxFit(nosNodeFunctions* funcs)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("BoxFit"), BoxFitNode, funcs);
	return NOS_RESULT_SUCCESS;
}
}