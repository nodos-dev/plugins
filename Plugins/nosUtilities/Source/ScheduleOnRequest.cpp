// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/PluginHelpers.hpp>
#include <nosVulkanSubsystem/nosVulkanSubsystem.h>

// stl
#include <chrono>
#include <nosUtil/Stopwatch.hpp>

#include "Sink_generated.h"

namespace nos::utilities
{

NOS_REGISTER_NAME(Request);
NOS_REGISTER_NAME(Input);
NOS_REGISTER_NAME(Output);
NOS_REGISTER_NAME(ScheduleWhenNodeCreated);
NOS_REGISTER_NAME(OnResponse);

struct ScheduleOnRequestNode : NodeContext
{
	ScheduleOnRequestNode(nosFbNodePtr node) :
		NodeContext(node) {
		for (auto* pin : *node->pins())
		{
			auto name = pin->name()->c_str();
			if (pin->data() && pin->data()->size()) {
				if (strcmp(name, NSN_ScheduleWhenNodeCreated.AsCStr()) == 0)
					if ((*(bool*)pin->data()->data()))
						nosEngine.CallNodeFunction(NodeId, NSN_Request);
			}
		}
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

	nosResult ExecuteNode(nosNodeExecuteParams* params) override
	{
		nos::NodeExecuteParams execParams(params);
		nosEngine.SetPinValueByName(NodeId, NSN_Output, *execParams[NSN_Input].Data);
		nosEngine.TriggerNodeEvent(NodeId, NSN_Output);
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
