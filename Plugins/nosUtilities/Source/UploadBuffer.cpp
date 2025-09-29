// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>

// External
#include <glm/glm.hpp> // TODO: Ring no longer needs glm::mat4 colormatrix. Remove this
#include <nosVulkanSubsystem/Helpers.hpp>

namespace nos::utilities
{
struct UploadBufferNodeContext : NodeContext
{
	TypedObjectRef<sys::vulkan::Semaphore> TransferSem{};
	uint64_t FrameNumber = 1;
	nosResult OnCreate(nosFbNodePtr node)
	{
		nosSemaphoreCreateInfo semCreateInfo{
			.Type = NOS_SEMAPHORE_TYPE_TIMELINE
		};
		return nosVulkan->CreateSemaphore(&semCreateInfo, &TransferSem.Handle);
	}
	nosResult ExecuteNode(NodeExecuteParams const& params) override
	{
		auto outBuf = params.GetPinObject<sys::vulkan::Buffer>(NOS_NAME_STATIC("Output"));
		auto inBuf = params.GetPinObject<sys::vulkan::Buffer>(NOS_NAME_STATIC("InputBuffer"));
		if (!inBuf.IsValid())
			return NOS_RESULT_FAILED;
		auto inGpuEventHolder = params.GetPinObject<sys::vulkan::GPUEventHolder>(NOS_NAME_STATIC("InputGPUEventRef"));
		nosVkGPUEvent* event = nullptr;
		if (inGpuEventHolder.IsValid())
		{
			auto res = nosVulkan->GetGPUEventFromHolder(inGpuEventHolder, &event);
			assert(res != NOS_RESULT_SUCCESS || *event == 0);
		}
		
		auto inInfo = *sys::vulkan::GetResourceInfo(inBuf);
		auto outBufInfo = sys::vulkan::GetResourceInfo(outBuf);
		if (!outBufInfo || outBufInfo->Size != inInfo.Size)
		{
			nosBufferInfo newBufInfo {.Size = inInfo.Size,
												 .Usage = nosBufferUsage(NOS_BUFFER_USAGE_TRANSFER_DST |
																		 NOS_BUFFER_USAGE_TRANSFER_SRC |
																		 NOS_BUFFER_USAGE_STORAGE_BUFFER),
												 .MemoryFlags = nosMemoryFlags(NOS_MEMORY_FLAGS_DEVICE_MEMORY)};

			outBuf = sys::vulkan::CreateBuffer(newBufInfo, "UploadBuffer Output");
			SetPinObject(NOS_NAME("Output"), outBuf);
		}

		if (!outBuf.IsValid())
			return NOS_RESULT_FAILED;

		nosVulkan->SetResourceFieldType(outBuf, inInfo.FieldType);

		{
			nosCmd cmd;
			nosCmdBeginParams cmdParams = { .Name = NOS_NAME("UploadBuffer Staging Copy"), .AssociatedNodeId = NodeId, .OutCmdHandle = &cmd,
				.PreferredQueueType = NOS_CMD_QUEUE_TYPE_TRANSFER,
			};
			auto res = nosVulkan->Begin(&cmdParams);
			nosVulkan->Copy(cmd, inBuf, outBuf, 0);
			nosVulkan->AddSignalSemaphoreToCmd(cmd, TransferSem, FrameNumber);
			nosCmdEndParams endParams{ .ForceSubmit = true, .OutGPUEventHandle = event };
			nosVulkan->End(cmd, &endParams);
		}
		{
			auto cmd = sys::vulkan::BeginCmd(NOS_NAME("Wait Transfer"), NodeId);
			nosVulkan->AddWaitSemaphoreToCmd(cmd, TransferSem, FrameNumber++);
			nosVulkan->End(cmd, nullptr);
		}

		return NOS_RESULT_SUCCESS;
	}

};

nosResult RegisterUploadBuffer(nosNodeFunctions* functions)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("UploadBuffer"), UploadBufferNodeContext, functions)
	return NOS_RESULT_SUCCESS;
}

}