// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>

#include <nosSysVulkan/Helpers.hpp>

namespace nos::experiment
{
NOS_REGISTER_NAME_SPACED(Nos_Utilities_PartialBufferCopy, "nos.test.PartialBufferCopy")

struct PartialBufferCopy : NodeContext
{
	using NodeContext::NodeContext;

	TypedObjectRef<sys::vulkan::Buffer> OutputBuffer;

	nosResult ExecuteNode(NodeExecuteParams const& params) override
	{
		auto input = params.GetPinObject<sys::vulkan::Buffer>(NOS_NAME("Input"));
		auto srcOffset = params.GetPinData<uint32_t>(NOS_NAME("SrcOffset"));
		auto dstOffset = params.GetPinData<uint32_t>(NOS_NAME("DstOffset"));
		auto size = params.GetPinData<uint32_t>(NOS_NAME("Size"));

		if (!input)
			return NOS_RESULT_INVALID_ARGUMENT;

		auto inputResInfo = sys::vulkan::GetResourceInfo(input);
		auto outputBufInfo = sys::vulkan::GetResourceInfo(OutputBuffer);
		if (!outputBufInfo || inputResInfo->Size != outputBufInfo->Size)
		{
			nosBufferInfo bufDesc{};
			bufDesc.Size = inputResInfo->Size;
			bufDesc.Usage = nosBufferUsage(NOS_BUFFER_USAGE_TRANSFER_SRC |
										   NOS_BUFFER_USAGE_TRANSFER_DST | 
										   NOS_BUFFER_USAGE_STORAGE_BUFFER);
			bufDesc.MemoryFlags = NOS_MEMORY_FLAGS_DEVICE_MEMORY;
			if (OutputBuffer = sys::vulkan::CreateBuffer(bufDesc, "PartialBufferCopy Output"))
			{
				nosVulkan->SetResourceFieldType(OutputBuffer, sys::vulkan::GetResourceFieldType(input));
				SetPinObject(NOS_NAME("Output"), OutputBuffer);
			}
			else
			{
				return NOS_RESULT_FAILED;
			}
		}
		auto output = params.GetPinObject<sys::vulkan::Buffer>(NOS_NAME("Output"));

		nosCopyParams copyParams{};
		copyParams.RegionCount = 1;
		nosCopyRegion region{};
		region.BufferCopy.SrcOffset = *srcOffset;
		region.BufferCopy.DstOffset = *dstOffset;
		region.BufferCopy.Size = *size;
		copyParams.Regions = &region;

		nosCmd cmd{};
		nosCmdBeginParams beginParams{ .Name = NOS_NAME("Partial Buffer Copy"), .AssociatedNodeId = NodeId, .OutCmdHandle = &cmd };
		nosVulkan->Begin(&beginParams);
		nosVulkan->Copy(cmd, input, output, &copyParams);
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