// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>

#include <nosSysVulkan/Helpers.hpp>

namespace nos::experiment
{
NOS_REGISTER_NAME_SPACED(Nos_Utilities_PartialBufferCopy, "nos.test.PartialBufferCopy")

struct PartialBufferCopy : NodeContext
{
	using NodeContext::NodeContext;

	std::optional<vkss::Resource> OutputBuffer;

	nosResult ExecuteNode(nosNodeExecuteParams* params) override
	{
		auto pins = GetPinValues(params);
		auto input = GetPinValue<sys::vulkan::Buffer>(pins, NOS_NAME("Input"));
		auto srcOffset = GetPinValue<uint32_t>(pins, NOS_NAME("SrcOffset"));
		auto dstOffset = GetPinValue<uint32_t>(pins, NOS_NAME("DstOffset"));
		auto size = GetPinValue<uint32_t>(pins, NOS_NAME("Size"));
		auto output = GetPinValue<sys::vulkan::Buffer>(pins, NOS_NAME("Output"));

		if (!input->handle())
			return NOS_RESULT_INVALID_ARGUMENT;

		if (!OutputBuffer || input->size_in_bytes() != OutputBuffer->Info.Buffer.Size)
		{
			nosBufferInfo bufDesc{};
			bufDesc.Size = input->size_in_bytes();
			bufDesc.Usage = nosBufferUsage(NOS_BUFFER_USAGE_TRANSFER_SRC |
										   NOS_BUFFER_USAGE_TRANSFER_DST | 
										   NOS_BUFFER_USAGE_STORAGE_BUFFER);
			bufDesc.MemoryFlags = NOS_MEMORY_FLAGS_DEVICE_MEMORY;
			if (OutputBuffer = vkss::Resource::Create(bufDesc, "PartialBufferCopy Output"))
			{
				auto outPinData = OutputBuffer->ToPinData();
				outPinData.As<sys::vulkan::Buffer>()->mutate_field_type(input->field_type());
				nosEngine.SetPinValueByName(NodeId, NOS_NAME("Output"), outPinData);
			}
			else
			{
				return NOS_RESULT_FAILED;
			}
		}

		nosCopyParams copyParams;
		copyParams.RegionCount = 1;
		nosCopyRegion region{};
		region.BufferCopy.SrcOffset = *srcOffset;
		region.BufferCopy.DstOffset = *dstOffset;
		region.BufferCopy.Size = *size;
		copyParams.Regions = &region;
		auto src = vkss::ConvertToResourceInfo(*input);
		auto dst = vkss::ConvertToResourceInfo(*output);

		nosCmd cmd{};
		nosCmdBeginParams beginParams{ .Name = NOS_NAME("Partial Buffer Copy"), .AssociatedNodeId = NodeId, .OutCmdHandle = &cmd };
		nosVulkan->Begin(&beginParams);
		nosVulkan->Copy(cmd, &src, &dst, &copyParams);
		nosVulkan->End(cmd, nullptr);
		return NOS_RESULT_SUCCESS;
	}
};

nosResult RegisterPartialBufferCopy(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NSN_Nos_Utilities_PartialBufferCopy, PartialBufferCopy, fn);
	return NOS_RESULT_SUCCESS;
}

} // namespace nos::test