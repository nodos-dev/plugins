// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/PluginHelpers.hpp>

#include <nosVulkanSubsystem/Helpers.hpp>

#include <memory>

namespace nos::utilities
{

struct ReduceTextureNode : NodeContext
{
	using NodeContext::NodeContext;

	nosResult OnCreate(nosFbNodePtr node) override
	{
		nosResourceShareInfo resultBufferCreateInfo{
			.Info = nosResourceInfo {
				.Type = NOS_RESOURCE_TYPE_BUFFER,
				.Buffer = nosBufferInfo {
					.Size = sizeof(float) * 2,
					.Usage = nosBufferUsage(NOS_BUFFER_USAGE_STORAGE_BUFFER | NOS_BUFFER_USAGE_TRANSFER_DST | NOS_BUFFER_USAGE_TRANSFER_SRC),
					.MemoryFlags = nosMemoryFlags(NOS_MEMORY_FLAGS_HOST_VISIBLE)
				}
			}
		};
		if (auto buf = vkss::Resource::Create(resultBufferCreateInfo, ("ReduceTextureNode" + uuids::to_string(NodeId)).c_str()))
		{
			ResultBuffer = std::make_unique<vkss::Resource>(std::move(*buf));
			return NOS_RESULT_SUCCESS;
		}
		else
		{
			nosEngine.LogE("Failed to create result buffer for ReduceTextureNode.");
			return NOS_RESULT_FAILED;
		}
	}

	static constexpr uint32_t GetElementSize(sys::vulkan::BufferElementType type) {
		switch (type) {
		case sys::vulkan::BufferElementType::ELEMENT_TYPE_FLOAT:
		case sys::vulkan::BufferElementType::ELEMENT_TYPE_INT32:
		case sys::vulkan::BufferElementType::ELEMENT_TYPE_UINT32:
			return 4;
		case sys::vulkan::BufferElementType::ELEMENT_TYPE_FLOAT16:
		case sys::vulkan::BufferElementType::ELEMENT_TYPE_INT16:
		case sys::vulkan::BufferElementType::ELEMENT_TYPE_UINT16:
			return 2;
		case sys::vulkan::BufferElementType::ELEMENT_TYPE_INT8:
		case sys::vulkan::BufferElementType::ELEMENT_TYPE_UINT8:
			return 1;
		default:
			return 4;  // Default fallback size
		}
	}

	bool MessageSet = false;

	nosResult ExecuteNode(nosNodeExecuteParams* params) override
	{
		static const nos::Name NSN_Input = NOS_NAME("Input");
		static const nos::Name NSN_Result = NOS_NAME("Result");

		nos::NodeExecuteParams nodeParams(params);
		nosResourceShareInfo inputTexDesc = vkss::DeserializeTextureInfo(nodeParams[NSN_Input].Data->Data);

		if (!inputTexDesc.Memory.Handle)
			return NOS_RESULT_FAILED;

		if (vkss::GetNumberOfComponentsFromTextureFormat(inputTexDesc.Info.Texture.Format) != 1)
		{
			if (!MessageSet)
			{
				SetNodeStatusMessage("Input texture must be a single channel texture (e.g., R32F).", nos::fb::NodeStatusMessageType::FAILURE);
				MessageSet = true;
			}
			return NOS_RESULT_FAILED;
		}

		if (MessageSet)
		{
			ClearNodeStatusMessages();
			MessageSet = false;
		}

		nosResourceShareInfo& resultBufferDesc = *ResultBuffer;

		// Reset the Result buffer to zero before running the compute pass
		auto* resultBufferData = reinterpret_cast<float*>(nosVulkan->Map(&resultBufferDesc));
		if (resultBufferData)
		{
			resultBufferData[0] = 0.0f; // Reset sum
			resultBufferData[1] = 0.0f; // Reset sum of squares
		}

		std::vector bindings = {
			vkss::ShaderBinding(NSN_Input, inputTexDesc),
			vkss::ShaderBinding(NSN_Result, resultBufferDesc)
		};

		uint32_t elementCount = inputTexDesc.Info.Texture.Width * inputTexDesc.Info.Texture.Height;
		uint32_t localSizeX = 256; // Workgroup size for X dimension
		uint32_t dispatchSizeX = (elementCount + localSizeX - 1) / localSizeX;  // ceil division

		nosRunComputePassParams pass{
			.Key = NOS_NAME("TEXTURE_REDUCE_PASS"),
			.Bindings = bindings.data(),
			.BindingCount = uint32_t(bindings.size()),
			.DispatchSize = { dispatchSizeX, 1 },
			.Benchmark = false
		};

		nosCmd cmd;
		nosCmdBeginParams beginParams{ .Name = NOS_NAME("TextureReducePass"), .AssociatedNodeId = params->NodeId, .OutCmdHandle = &cmd };
		nosVulkan->Begin(&beginParams);

		nosVulkan->RunComputePass(cmd, &pass);
		nosGPUEvent waitEvent{};
		nosCmdEndParams endParams{ .ForceSubmit = NOS_TRUE, .OutGPUEventHandle = &waitEvent };
		nosVulkan->End(cmd, &endParams);
		nosVulkan->WaitGpuEvent(&waitEvent, UINT64_MAX);

		auto* result = reinterpret_cast<float*>(nosVulkan->Map(&resultBufferDesc));
		auto sum = result[0];
		auto sumOfSquares = result[1];
		auto mean = sum / float(elementCount);
		auto meanOfSquares = sumOfSquares / float(elementCount);

		nosEngine.SetPinValueByName(NodeId, NOS_NAME("Sum"), nos::Buffer::From(sum));
		nosEngine.SetPinValueByName(NodeId, NOS_NAME("Mean"), nos::Buffer::From(mean));
		nosEngine.SetPinValueByName(NodeId, NOS_NAME("SumOfSquares"), nos::Buffer::From(sumOfSquares));
		nosEngine.SetPinValueByName(NodeId, NOS_NAME("MeanOfSquares"), nos::Buffer::From(meanOfSquares));

		return NOS_RESULT_SUCCESS;
	}

	std::unique_ptr<vkss::Resource> ResultBuffer = nullptr;
};

nosResult RegisterReduceTexture(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("ReduceTexture"), ReduceTextureNode, fn);
	return NOS_RESULT_SUCCESS;
}

} // namespace nos::utilities