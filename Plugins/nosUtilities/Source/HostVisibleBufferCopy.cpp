// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>

#include <nosVulkanSubsystem/Helpers.hpp>

namespace nos::utilities
{
struct HostVisibleBufferCopyNode : NodeContext
{
	using NodeContext::NodeContext;
	bool ErrorSet = false;

	nosResult OnCreate(nosFbNodePtr node) override
	{
		ClearNodeStatusMessages();
		return NOS_RESULT_SUCCESS;
	}

	nosResult ExecuteNode(NodeExecuteParams const& params) override
	{
		auto source = params.GetPinObject<sys::vulkan::Buffer>(NOS_NAME("Source"));
		auto destination = params.GetPinObject<sys::vulkan::Buffer>(NOS_NAME("Destination"));
		auto srcBuf = nosVulkan->Map(source);
		auto dstBuf = nosVulkan->Map(destination);
		if (!srcBuf || !dstBuf)
		{
			if (!ErrorSet)
			{
				SetNodeStatusMessage("Both source and destination buffers must be host visible to perform copy", fb::NodeStatusMessageType::FAILURE);
				ErrorSet = true;
			}
			return NOS_RESULT_FAILURE;
		}
		auto srcInfo = sys::vulkan::GetResourceInfo(source);
		auto dstInfo = sys::vulkan::GetResourceInfo(destination);
		if (!srcInfo || !dstInfo)
		{
			if (!ErrorSet)
			{
				SetNodeStatusMessage("Failed to get buffer info for source or destination buffer", fb::NodeStatusMessageType::FAILURE);
				ErrorSet = true;
			}
			return NOS_RESULT_FAILURE;
		}
		auto copySize = std::min(srcInfo->Size, dstInfo->Size);
		std::memcpy(dstBuf, srcBuf, copySize);
		if (ErrorSet)
		{
			ClearNodeStatusMessages();
			ErrorSet = false;
		}
		SetPinObject(NOS_NAME("OutDestination"), destination);
		return NOS_RESULT_SUCCESS;
	}
};

nosResult RegisterHostVisibleBufferCopy(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("HostVisibleBufferCopy"), HostVisibleBufferCopyNode, fn);
	return NOS_RESULT_SUCCESS;
}

} // namespace nos::utilities