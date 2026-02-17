// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>
#include <nosSysVulkan/Helpers.hpp>
namespace nos::flow
{
NOS_REGISTER_NAME_SPACED(Nos_Utilities_RepeatingJunction, "nos.flow.RepeatingJunction")

/// This node lets the pulling paths execute even if there is no input data available yet, in a safe manner.
struct RepeatingJunctionNode : NodeContext
{
	std::mutex NewFrameMutex;
	std::optional<ObjectRef> NewFrameObject;

	nosResult ExecuteNode(NodeExecuteParams const& params) override
	{
		ObjectRef inputObject = params.GetPinObject(NOS_NAME("Input"));
		bool waitGpu = *params.GetPinData<bool>(NOS_NAME("WaitGPU"));
		SubmitAndWaitIfWanted(waitGpu);
		{
			std::unique_lock lock(NewFrameMutex);
			NewFrameObject = std::move(inputObject);
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

	nosResult CopyFrom(nosCopyFromInfo* cpy) override 
	{
		std::unique_lock lock(NewFrameMutex);
		if (NewFrameObject)
		{
			SetPinObject(NOS_NAME("Output"), *NewFrameObject);
			NewFrameObject = std::nullopt;
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

} // namespace nos::flow