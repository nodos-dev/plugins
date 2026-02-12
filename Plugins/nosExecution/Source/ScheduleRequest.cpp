#include <Nodos/Plugin.hpp>

namespace nos::execution
{
struct ScheduleRequestNode : NodeContext
{
	bool TryAgainOnFailure = false;
	fb::vec2u DeltaSeconds = { 1, 60 };
	uint32_t Importance = 1;

	void OnPinObjectChanged(nos::Name pinName, const uuid& pinId, nosObjectId newHandle) override
	{
		if (pinName == NOS_NAME("DeltaSeconds"))
		{
			if (auto objBuf = GetObjectDataView(newHandle))
			{
				auto& val = *static_cast<const fb::vec2u*>(objBuf->Data);
				if (val != DeltaSeconds)
				{
					DeltaSeconds = val;
					SendPathRestart(NOS_NAME("Trigger"));
				}
			}
		}
		else if (pinName == NOS_NAME("Importance"))
		{
			if (auto objBuf = GetObjectDataView(newHandle))
			{
				auto& val = *static_cast<const uint32_t*>(objBuf->Data);
				if (val != Importance)
				{
					Importance = val;
					SendPathRestart(NOS_NAME("Trigger"));
				}
			}
		}
		else if (pinName == NOS_NAME("TryAgainOnFailure"))
		{
			if (auto objBuf = GetObjectDataView(newHandle))
			{
				auto& val = *static_cast<const bool*>(objBuf->Data);
				if (val != TryAgainOnFailure)
				{
					TryAgainOnFailure = val;
					SendPathRestart(NOS_NAME("Trigger"));
				}
			}
		}
	}

	void GetScheduleInfo(nosScheduleInfo* info) override
	{
		info->Type = NOS_SCHEDULE_TYPE_ON_DEMAND;
		info->DeltaSeconds = reinterpret_cast<nosVec2u&>(DeltaSeconds);
		info->Importance = Importance;
	}

	nosResult ExecuteNode(NodeExecuteParams const& params) override
	{
		SendScheduleRequest(1);
		return NOS_RESULT_SUCCESS;
	}

	void OnPathStart() override
	{
		SendScheduleRequest(1);
	}

	void OnEndFrame(const uuid& pinId, nosEndFrameCause cause) override
	{
		if (TryAgainOnFailure && cause == NOS_END_FRAME_FAILED)
		{
			SendScheduleRequest(1);
		}
	}
};

nosResult RegisterScheduleRequest(nosNodeFunctions* funcs)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("ScheduleRequest"), ScheduleRequestNode, funcs);
	return NOS_RESULT_SUCCESS;
}
}