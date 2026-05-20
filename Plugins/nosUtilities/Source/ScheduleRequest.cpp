// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/PluginHelpers.hpp>

namespace nos::utilities
{

// Drives an on-demand path: each execution (and each path start) queues another
// schedule request, so the path feeding the Trigger pin keeps running. Wire the
// thing you want scheduled into Sink. Ported from nos.flow (dev branch).
struct ScheduleRequestNode : NodeContext
{
	bool TryAgainOnFailure = true;
	nosVec2u DeltaSeconds = { 1, 60 };
	uint32_t Importance = 1;

	ScheduleRequestNode(nosFbNodePtr node) : NodeContext(node)
	{
		if (node->pins())
			for (auto* pin : *node->pins())
			{
				auto* data = pin->data();
				if (data && data->size())
					ReadPin(nos::Name(pin->name()->c_str()), data->data());
			}
	}

	void ReadPin(nos::Name name, const void* data)
	{
		if (name == NOS_NAME("DeltaSeconds"))
			DeltaSeconds = *static_cast<const nosVec2u*>(data);
		else if (name == NOS_NAME("Importance"))
			Importance = *static_cast<const uint32_t*>(data);
		else if (name == NOS_NAME("TryAgainOnFailure"))
			TryAgainOnFailure = *static_cast<const bool*>(data);
	}

	void OnPinValueChanged(nos::Name pinName, uuid const& pinId, nosBuffer value) override
	{
		ReadPin(pinName, value.Data);
	}

	void GetScheduleInfo(nosScheduleInfo* info) override
	{
		info->Type = NOS_SCHEDULE_TYPE_ON_DEMAND;
		info->DeltaSeconds = DeltaSeconds;
		info->Importance = Importance;
	}

	void ScheduleOnce()
	{
		nosScheduleNodeParams params{ .NodeId = NodeId, .AddScheduleCount = 1, .Reset = false };
		nosEngine.ScheduleNode(&params);
	}

	nosResult ExecuteNode(nosNodeExecuteParams* params) override
	{
		ScheduleOnce();
		return NOS_RESULT_SUCCESS;
	}

	void OnPathStart() override
	{
		ScheduleOnce();
	}

	void OnEndFrame(uuid const& pinId, nosEndFrameCause cause) override
	{
		if (TryAgainOnFailure && cause == NOS_END_FRAME_FAILED)
			ScheduleOnce();
	}
};

nosResult RegisterScheduleRequest(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("ScheduleRequest"), ScheduleRequestNode, fn);
	return NOS_RESULT_SUCCESS;
}

} // namespace nos::utilities
