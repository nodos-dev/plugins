// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/PluginHelpers.hpp>

// External
#include <glm/glm.hpp> // TODO: Ring no longer needs glm::mat4 colormatrix. Remove this
#include <nosVulkanSubsystem/Helpers.hpp>

namespace nos::utilities
{
struct UploadBufferNodeContext : NodeContext
{
	nosSemaphore TransferSem;
	uint64_t FrameNumber = 1;
	nosResult OnCreate(nosFbNodePtr node) {
		nosSemaphoreCreateInfo semCreateInfo{
			.Type = NOS_SEMAPHORE_TYPE_TIMELINE
		};
		return nosVulkan->CreateSemaphore(&semCreateInfo, &TransferSem);
	}
	nosResult OnDestroy() {
		if (TransferSem)
		{
			nosVulkan->DestroySemaphore(&TransferSem);
			TransferSem = 0;
		}
		return NOS_RESULT_SUCCESS;
	}
	nosResult ExecuteNode(nosNodeExecuteParams* params) override
	{
		auto execParams = nos::NodeExecuteParams(params);
		auto& output = *InterpretPinValue<sys::vulkan::Buffer>(execParams[NOS_NAME_STATIC("Output")].Data->Data);
		auto& input = *InterpretPinValue<sys::vulkan::Buffer>(execParams[NOS_NAME_STATIC("InputBuffer")].Data->Data);
		nosGPUEventResource gpuEventRef = InterpretPinValue<sys::vulkan::GPUEventResource>(*execParams[NOS_NAME_STATIC("InputGPUEventRef")].Data)->handle();
		nosGPUEvent* event = nullptr;
		if (gpuEventRef)
		{
			auto res = nosVulkan->GetGPUEvent(gpuEventRef, &event);
			assert(res != NOS_RESULT_SUCCESS || *event == 0);
		}
		
		output.mutate_field_type(input.field_type());

		if (input.size_in_bytes() != output.size_in_bytes())
		{
			nosResourceShareInfo bufInfo = {
				.Info = {.Type = NOS_RESOURCE_TYPE_BUFFER,
						 .Buffer = nosBufferInfo{.Size = (uint32_t)input.size_in_bytes(),
												 .Usage = nosBufferUsage(NOS_BUFFER_USAGE_TRANSFER_DST |
																		 NOS_BUFFER_USAGE_TRANSFER_SRC |
																		 NOS_BUFFER_USAGE_STORAGE_BUFFER),
												 .MemoryFlags = nosMemoryFlags(NOS_MEMORY_FLAGS_DEVICE_MEMORY)}}};
			auto bufferDesc = vkss::ConvertBufferInfo(bufInfo);
			nosEngine.SetPinValueByName(NodeId, NOS_NAME_STATIC("Output"), Buffer::From(bufferDesc));

			output = *InterpretPinValue<sys::vulkan::Buffer>(execParams[NOS_NAME_STATIC("Output")].Data->Data);
		}

		if (!output.handle() || !input.handle())
		{
			return NOS_RESULT_SUCCESS;
		}

		auto OutputBuffer = vkss::ConvertToResourceInfo(output);
		auto InputBuffer = vkss::ConvertToResourceInfo(input);

		{
			nosCmd cmd;
			nosCmdBeginParams cmdParams = { .Name = NOS_NAME("UploadBuffer Staging Copy"), .AssociatedNodeId = NodeId, .OutCmdHandle = &cmd,
				.PreferredQueueType = NOS_CMD_QUEUE_TYPE_TRANSFER,
			};
			auto res = nosVulkan->Begin(&cmdParams);
			nosVulkan->Copy(cmd, &InputBuffer, &OutputBuffer, 0);
			nosVulkan->AddSignalSemaphoreToCmd(cmd, TransferSem, FrameNumber);
			nosCmdEndParams endParams{ .ForceSubmit = true, .OutGPUEventHandle = event };
			nosVulkan->End(cmd, &endParams);
		}
		{
			auto cmd = vkss::BeginCmd(NOS_NAME("Wait Transfer"), NodeId);
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