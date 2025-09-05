// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/PluginHelpers.hpp>
#include <nosVulkanSubsystem/Helpers.hpp>
namespace nos::utilities
{
NOS_REGISTER_NAME_SPACED(Nos_Utilities_RepeatingJunction, "nos.utilities.RepeatingJunction")

struct RepeatingJunctionNode : NodeContext
{
	using NodeContext::NodeContext;

	bool WaitGPU = true;

	std::mutex NewFrameMutex;
	std::optional<nos::Buffer> NewFrame;

	nosResult ExecuteNode(nosNodeExecuteParams* params) override
	{
		const nosBuffer* inputBuffer = nullptr;
		for (int i = 0; i < params->PinCount; ++i)
		{
			if (params->Pins[i].Name == NOS_NAME("Input"))
			{
				inputBuffer = params->Pins[i].Data;
			}
			else if (params->Pins[i].Name == NOS_NAME("WaitGPU"))
			{
				WaitGPU = *static_cast<const bool*>(params->Pins[i].Data->Data);
			}
		}
		SubmitAndWaitIfWanted();
		assert(inputBuffer);
		{
			std::unique_lock lock(NewFrameMutex);
			NewFrame = nos::Buffer(*inputBuffer);
		}
		return NOS_RESULT_SUCCESS;
	}

	void SubmitAndWaitIfWanted()
	{
		nosCmd cmd{};
		nosCmdBeginParams beginParams = { .Name = NOS_NAME("Repeating Junction Submit"), .AssociatedNodeId = NodeId, .OutCmdHandle = &cmd };
		nosVulkan->Begin(&beginParams);
		nosGPUEvent event{};
		nosCmdEndParams endParams{ .ForceSubmit = true, .OutGPUEventHandle = WaitGPU ? &event : nullptr };
		nosVulkan->End(cmd, &endParams);
		if (WaitGPU)
		{
			nosVulkan->WaitGpuEvent(&event, 1000000000);
		}
	}

	nosResult CopyFrom(nosCopyInfo* cpy) override {

		std::unique_lock lock(NewFrameMutex);
		if (NewFrame)
		{
			SetPinValue(NOS_NAME("Output"), *NewFrame);
			NewFrame = std::nullopt;
			ScheduleNextFrame();
		}
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