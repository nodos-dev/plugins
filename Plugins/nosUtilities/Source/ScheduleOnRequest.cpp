// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>
#include <nosVulkanSubsystem/nosVulkanSubsystem.h>

// stl
#include <chrono>
#include <Nodos/Utils/Stopwatch.hpp>

#include "Sink_generated.h"

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
		AddPinValueWatcher(NSN_ScheduleWhenNodeCreated, [this](nos::Buffer const& newVal, std::optional<nos::Buffer> oldVal) {
			if (!oldVal)
				ExecuteForOnceAtCreation = *InterpretPinValue<bool>(newVal);
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

	nosResult ExecuteNode(nosNodeExecuteParams* params) override
	{
		nos::NodeExecuteParams execParams(params);
		nosEngine.SetPinValueByName(NodeId, NSN_Output, *execParams[NSN_Input].Data);
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
