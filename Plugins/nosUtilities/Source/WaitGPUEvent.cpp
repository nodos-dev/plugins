// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>

// External
#include <glm/glm.hpp> // TODO: Ring no longer needs glm::mat4 colormatrix. Remove this
#include <nosVulkanSubsystem/Helpers.hpp>

namespace nos::utilities
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
		nosGPUEvent* event = nullptr;
		if (inGpuEventHolder.IsValid())
		{
			auto res = nosVulkan->GetGPUEventFromHolder(inGpuEventHolder, &event);
			assert(res != NOS_RESULT_SUCCESS || *event == 0);
		}
		if (event && *event)
		{
			nosVulkan->WaitGpuEvent(event, UINT64_MAX);
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