// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>
#include <nosVulkanSubsystem/Helpers.hpp>
namespace nos::utilities
{
NOS_REGISTER_NAME_SPACED(Nos_Utilities_RepeatingJunction, "nos.utilities.RepeatingJunction")

/// This node lets the pulling paths execute even if there is no input data available yet, in a safe manner.
struct RepeatingJunctionNode : NodeContext
{
	std::mutex NewFrameMutex;
	std::optional<nos::Buffer> NewFrame;

	nosResult ExecuteNode(nosNodeExecuteParams* args) override
	{
		NodeExecuteParams params(args);
		nos::Buffer inputBuffer = *params[NOS_NAME("Input")].Data;
		bool waitGpu = *params.GetPinData<bool>(NOS_NAME("WaitGPU"));
		SubmitAndWaitIfWanted(waitGpu);
		{
			std::unique_lock lock(NewFrameMutex);
			NewFrame = std::move(inputBuffer);
		}
		return NOS_RESULT_SUCCESS;
	}

	void SubmitAndWaitIfWanted(bool waitGPU)
	{
		nosCmd cmd{};
		nosCmdBeginParams beginParams = { .Name = NOS_NAME("Repeating Junction Submit"), .AssociatedNodeId = NodeId, .OutCmdHandle = &cmd };
		nosVulkan->Begin(&beginParams);
		nosGPUEvent event{};
		nosCmdEndParams endParams{.ForceSubmit = true, .OutGPUEventHandle = waitGPU ? &event : nullptr};
		nosVulkan->End(cmd, &endParams);
		if (waitGPU)
		{
			nosVulkan->WaitGpuEvent(&event, 1000000000);
		}
	}

	nosResult CopyFrom(nosCopyInfo* cpy) override 
	{
		std::unique_lock lock(NewFrameMutex);
		if (NewFrame)
		{
			SetPinValue(NOS_NAME("Output"), *NewFrame);
			NewFrame = std::nullopt;
			ScheduleNextFrame();
		}
		return NOS_RESULT_SUCCESS;
	}

	void OnPathStart() override
	{
		ScheduleNextFrame();
	}

	void ScheduleNextFrame()
	{
		nosScheduleNodeParams schedule{
			.NodeId = NodeId,
			.AddScheduleCount = 1
		};
		nosEngine.ScheduleNode(&schedule);
	}
};

nosResult RegisterRepeatingJunction(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NSN_Nos_Utilities_RepeatingJunction, RepeatingJunctionNode, fn);
	return NOS_RESULT_SUCCESS;
}

} // namespace nos::utilities