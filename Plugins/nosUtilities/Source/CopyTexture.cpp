// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>

#include <nosVulkanSubsystem/Helpers.hpp>

namespace nos::utilities
{
struct CopyTextureNode : NodeContext
{
	using NodeContext::NodeContext;

	nosResult ExecuteNode(NodeExecuteParams const& params) override
	{
		auto source = params.GetPinObject<sys::vulkan::Texture>(NOS_NAME("Source"));
		auto destination = params.GetPinObject<sys::vulkan::Texture>(NOS_NAME("Destination"));
		auto inEventHolder = params.GetPinObject<sys::vulkan::GPUEventHolder>(NOS_NAME("InGPUEventHolder"));
		bool preferTransferQueue = *params.GetPinData<bool>(NOS_NAME("PreferTransferQueue"));
		nosCmd cmd{};
		nosCmdBeginParams beginParams{
			.Name = NOS_NAME("Texture Copy"),
			.AssociatedNodeId = NodeId,
			.OutCmdHandle = &cmd,
			.PreferredQueueType = preferTransferQueue ? NOS_CMD_QUEUE_TYPE_TRANSFER : NOS_CMD_QUEUE_TYPE_MAIN,
		};
		nosVulkan->Begin(&beginParams);
		nosVulkan->Copy(cmd, source, destination, nullptr);
		nosCmdEndParams endParams{};
		if (inEventHolder)
		{
			nosGPUEvent* inEvent = nullptr;
			nosVulkan->GetGPUEventFromHolder(inEventHolder, &inEvent);
			if (inEvent)
			{
				endParams.ForceSubmit = NOS_TRUE;
				endParams.OutGPUEventHandle = inEvent;
			}
		}
		nosVulkan->End(cmd, &endParams);
		SetPinObject(NOS_NAME("OutGPUEventHolder"), inEventHolder);
		SetPinObject(NOS_NAME("OutDestination"), destination);
		return NOS_RESULT_SUCCESS;
	}
};

nosResult RegisterCopyTexture(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("CopyTexture"), CopyTextureNode, fn);
	return NOS_RESULT_SUCCESS;
}

} // namespace nos::utilities