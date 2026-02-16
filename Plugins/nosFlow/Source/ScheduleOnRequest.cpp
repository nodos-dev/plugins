// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>
#include <nosSysVulkan/nosVulkanSubsystem.h>

// stl
#include <chrono>
#include <Nodos/Utils/Stopwatch.hpp>

#include "nosFlow/Sink_generated.h"

namespace nos::utilities
{

NOS_REGISTER_NAME(Request);
NOS_REGISTER_NAME(Input);
NOS_REGISTER_NAME(Output);
NOS_REGISTER_NAME(OnResponse);
NOS_REGISTER_NAME(ScheduleWhenNodeCreated);

struct ScheduleOnRequestNode : NodeContext
{
	// This variable is only used to execute the node if the ScheduleWhenNodeCreated pin is set to true at construction
	bool ExecuteForOnceAtCreation = false;
	nosResult OnCreate(nosFbNodePtr node) 
	{
		// Listen to the ScheduleWhenNodeCreated pin only at construction
		AddPinValueWatcher<bool>(NSN_ScheduleWhenNodeCreated,
								 [this](const bool* newVal, std::optional<const bool*> oldVal) {
			if (!oldVal.has_value())
				ExecuteForOnceAtCreation = *newVal;
		});
		return NOS_RESULT_SUCCESS;
	}

	void ScheduleNode()
	{
		nosScheduleNodeParams schedule {
			.NodeId = NodeId,
			.AddScheduleCount = 1,
			.Reset = NOS_FALSE,
		};
		nosEngine.ScheduleNode(&schedule);
	}

	void GetScheduleInfo(nosScheduleInfo* info) override
	{
		info->Type = NOS_SCHEDULE_TYPE_ON_DEMAND;
		info->DeltaSeconds = { 0, 0 };
		info->Importance = 0;
	}

	nosResult Request(nosFunctionExecuteParams* params) {
		nosEngine.SetPinValueByName(NodeId, NSN_ScheduleWhenNodeCreated, nos::Buffer::From(true));
		ScheduleNode();
		return NOS_RESULT_SUCCESS;
	}

	void OnPathStart() override {
		if (ExecuteForOnceAtCreation)
			ScheduleNode();
	}

	nosResult ExecuteNode(NodeExecuteParams const& params) override
	{
		SetPinValue(NSN_Output, params.GetPinObject(NSN_Input));
		nosEngine.TriggerNodeEvent(NodeId, NSN_OnResponse);
		ExecuteForOnceAtCreation = false;
		return NOS_RESULT_SUCCESS;
	}

	NOS_DECLARE_FUNCTIONS(
		NOS_ADD_FUNCTION(NSN_Request, Request))
};

nosResult RegisterScheduleOnRequest(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("ScheduleOnRequest"), ScheduleOnRequestNode, fn)
	return NOS_RESULT_SUCCESS;
}
}
