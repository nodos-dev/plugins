// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#pragma once

#include <Nodos/Plugin.hpp>

// External
#include <glm/glm.hpp> // TODO: Ring no longer needs glm::mat4 colormatrix. Remove this
#include <nosSysVulkan/Helpers.hpp>

#include "Ring.h"
#include "Nodos/Utils/Stopwatch.hpp"

namespace nos::utilities
{

struct BoundedQueueNodeContext : RingNodeBase
{
	BoundedQueueNodeContext() : RingNodeBase(RingNodeBase::OnRestartType::RESET)
	{
	}
	std::string GetName() const override
	{
		return "BoundedQueue";
	}
	
	nosResult CopyFrom(nosCopyInfo* cpy) override {
		ResourceInterface::ResourceBase* slot = nullptr;
		auto beginResult = CommonCopyFrom(cpy, &slot);
		if(beginResult != NOS_RESULT_SUCCESS || !slot)
			return beginResult;

		Ring->ResInterface->Copy(slot, cpy, NodeId);

		cpy->CopyFromOptions.ShouldSetSourceFrameNumber = true;
		cpy->FrameNumber = slot->FrameNumber;

		Ring->EndPop(slot);
		SendRingStats("End Copy From");
		SendScheduleRequest(1);
		return NOS_RESULT_SUCCESS;
	}

	nosResult ExecuteNode(nosNodeExecuteParams* params) override {
		return ExecuteRingNode(params, false, NOS_NAME_STATIC("BoundedQueue"), false);
	}
};

nosResult RegisterBoundedQueue(nosNodeFunctions* functions)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("BoundedQueue"), BoundedQueueNodeContext, functions)
		return NOS_RESULT_SUCCESS;
}


}