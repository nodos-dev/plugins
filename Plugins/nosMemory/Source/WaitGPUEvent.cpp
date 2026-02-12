// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>

// External
#include <glm/glm.hpp> // TODO: Ring no longer needs glm::mat4 colormatrix. Remove this
#include <nosSysVulkan/Helpers.hpp>

namespace nos::memory
{
struct WaitGPUEventNode : NodeContext
{
	nosResult OnCreate(nosFbNodePtr node) override
	{
		return NOS_RESULT_SUCCESS;
	}
	nosResult ExecuteNode(NodeExecuteParams const& params) override
	{
		auto inGpuEventHolder = params.GetPinObject<sys::vulkan::GPUEventHolder>(NOS_NAME_STATIC("Event"));
		auto timeoutMs = *params.GetPinData<uint32_t>(NOS_NAME_STATIC("TimeoutMs"));
		nosGPUEvent* event = nullptr;
		if (inGpuEventHolder.IsValid())
		{
			auto res = nosVulkan->GetGPUEventFromHolder(inGpuEventHolder, &event);
			// NOS_SOFT_CHECK(res == NOS_RESULT_SUCCESS && *event != 0, "Failed to get event");
		}
		if (event && *event)
		{
			auto res = nosVulkan->WaitGpuEvent(event, timeoutMs * 1'000'000);
			if (res == NOS_RESULT_TIMEOUT)
				return NOS_RESULT_PENDING;
			return res;
		}
		return NOS_RESULT_SUCCESS;
	}
};

nosResult RegisterWaitGPUEvent(nosNodeFunctions* functions)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("WaitGPUEvent"), WaitGPUEventNode, functions)
	return NOS_RESULT_SUCCESS;
}
}