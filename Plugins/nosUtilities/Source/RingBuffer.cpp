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

struct RingBufferNodeContext : RingNodeBase
{
	RingBufferNodeContext() : RingNodeBase(RingNodeBase::OnRestartType::WAIT_UNTIL_FULL)
	{
	}
	~RingBufferNodeContext()
	{ 
		NOS_SOFT_CHECK(LastPopped == nullptr, "LastPopped is not nullptr");
	}

	ResourceInterface::ResourceBase* LastPopped = nullptr;

	std::string GetName() const override
	{
		return "RingBuffer";
	}

	nosResult CopyFrom(nosCopyFromInfo* cpy) override {
		NOS_SOFT_CHECK(LastPopped == nullptr, "LastPopped is not nullptr");
		ResourceInterface::ResourceBase* slot = nullptr;
		auto beginResult = CommonCopyFrom(cpy, &slot);
		if (beginResult != NOS_RESULT_SUCCESS || !slot)
			return beginResult;

		Ring->ResInterface->WaitForDownloadToEnd(slot, "RingBuffer", NodeName.AsString(), cpy);

		cpy->ShouldSetSourceFrameNumber = true;
		cpy->FrameNumber = slot->FrameNumber;

		LastPopped = slot;
		SendScheduleRequest(1);
		return NOS_RESULT_SUCCESS;
	}

	nosResult ExecuteNode(NodeExecuteParams const& params) override {
		return ExecuteRingNode(params, true, NOS_NAME_STATIC("RingBuffer"), true);
	}

	void OnEndFrame(uuid const& pinId, nosEndFrameCause cause) override
	{
		RingNodeBase::OnEndFrame(pinId, cause);
		if (pinId == PinName2Id[NOS_NAME_STATIC("Output")])
		{
			if (!LastPopped)
				return;
			Ring->EndPop(LastPopped);
			LastPopped = nullptr;
			SendRingStats("End Frame");
		}
	}
};

nosResult RegisterRingBuffer(nosNodeFunctions* functions)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("RingBuffer"), RingBufferNodeContext, functions)
		return NOS_RESULT_SUCCESS;
}


}