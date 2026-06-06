// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/PluginHelpers.hpp>

#include <chrono>
#include <thread>

namespace nos::utilities
{

// Matches nos.utilities.ScheduleRequestMode in ScheduleRequest.fbs.
enum class ScheduleRequestMode : uint32_t
{
	Continuous = 0,
	FixedStep = 1,
};

// Drives an on-demand path: each execution (and each path start) queues another
// request, so the path feeding Trigger keeps running. Wire what you want scheduled
// into Sink. Ported from nos.flow (dev branch).
//
// FixedStep paces the path to even DeltaSeconds intervals. Deadlines are absolute
// (anchored at path start, advanced one delta per run), so upstream time is absorbed
// into the interval rather than added on top.
struct ScheduleRequestNode : NodeContext
{
	using Clock = std::chrono::steady_clock;

	bool TryAgainOnFailure = true;
	nosVec2u DeltaSeconds = { 1, 60 };
	uint32_t Importance = 1;
	ScheduleRequestMode Mode = ScheduleRequestMode::Continuous;
	float MaxWaitSeconds = 1.0f;

	// FixedStep pacing state.
	bool HasDeadline = false;
	Clock::time_point NextDeadline;

	enum class Status
	{
		Ok,
		InvalidDeltaSeconds,
		WaitCapped,
	};
	Status CurrentStatus = Status::Ok;

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
		{
			auto v = *static_cast<const nosVec2u*>(data);
			if (v.x != 0 && v.y != 0)
			{
				DeltaSeconds = v;
				SetStatus(Status::Ok);
			}
			else
				SetStatus(Status::InvalidDeltaSeconds);
		}
		else if (name == NOS_NAME("Importance"))
			Importance = *static_cast<const uint32_t*>(data);
		else if (name == NOS_NAME("TryAgainOnFailure"))
			TryAgainOnFailure = *static_cast<const bool*>(data);
		else if (name == NOS_NAME("Mode"))
			Mode = static_cast<ScheduleRequestMode>(*static_cast<const uint32_t*>(data));
		else if (name == NOS_NAME("MaxWaitSeconds"))
			MaxWaitSeconds = *static_cast<const float*>(data);
	}

	void OnPinValueChanged(nos::Name pinName, uuid const& pinId, nosBuffer value) override
	{
		ReadPin(pinName, value.Data);
		if (pinName == NOS_NAME("DeltaSeconds") || pinName == NOS_NAME("Mode"))
			ResetSchedule();
	}

	void SetStatus(Status status)
	{
		if (status == CurrentStatus)
			return;
		CurrentStatus = status;
		switch (status)
		{
		case Status::Ok: ClearNodeStatusMessages(); break;
		case Status::InvalidDeltaSeconds:
			SetNodeStatusMessage("Delta Seconds must be non-zero", fb::NodeStatusMessageType::WARNING);
			break;
		case Status::WaitCapped:
			SetNodeStatusMessage("Delta Seconds exceeds Max Wait Seconds; raise it to run at the configured rate",
								 fb::NodeStatusMessageType::WARNING);
			break;
		}
	}

	void GetScheduleInfo(nosScheduleInfo* info) override
	{
		info->Type = NOS_SCHEDULE_TYPE_ON_DEMAND;
		info->DeltaSeconds = DeltaSeconds;
		info->Importance = Importance;
	}

	std::chrono::duration<double> DeltaInterval() const
	{
		if (DeltaSeconds.x == 0 || DeltaSeconds.y == 0)
			return std::chrono::duration<double>::zero();
		return std::chrono::duration<double>(static_cast<double>(DeltaSeconds.x) /
											 static_cast<double>(DeltaSeconds.y));
	}

	// Tail we busy-wait instead of sleeping, sized to cover the OS timer overshoot.
	static constexpr auto SpinWindow = std::chrono::milliseconds(20);

	// Sleep while comfortably ahead, then busy-wait the deadline. Only sleep if the
	// interval can spare two spin windows; otherwise spin the whole way.
	static void WaitUntil(Clock::time_point deadline, std::chrono::duration<double> interval)
	{
		if (interval >= 2 * SpinWindow)
			for (auto remaining = deadline - Clock::now(); remaining > SpinWindow;
				 remaining = deadline - Clock::now())
				std::this_thread::sleep_for(remaining - SpinWindow);
		while (Clock::now() < deadline)
			; // busy-wait
	}

	// Wait for this run's slot, then advance the deadline. The first run only anchors.
	void PaceFixedStep()
	{
		auto interval = DeltaInterval();
		auto now = Clock::now();
		if (!HasDeadline || interval <= std::chrono::duration<double>::zero())
		{
			NextDeadline = now + std::chrono::duration_cast<Clock::duration>(interval);
			HasDeadline = true;
			return;
		}

		// Cap how long a single run may block so a bad Delta Seconds can't stall the
		// engine thread. 0 disables the cap.
		auto deadline = NextDeadline;
		bool capped = false;
		if (MaxWaitSeconds > 0.0f)
		{
			auto cap = now + std::chrono::duration_cast<Clock::duration>(
								   std::chrono::duration<float>(MaxWaitSeconds));
			if (deadline > cap)
			{
				deadline = cap;
				capped = true;
			}
		}
		SetStatus(capped ? Status::WaitCapped : Status::Ok);

		WaitUntil(deadline, interval);
		NextDeadline += std::chrono::duration_cast<Clock::duration>(interval);

		// Drop slots missed by upstream overrun to stay phase-aligned.
		now = Clock::now();
		while (NextDeadline < now)
			NextDeadline += std::chrono::duration_cast<Clock::duration>(interval);
	}

	void ScheduleOnce()
	{
		nosScheduleNodeParams params{ .NodeId = NodeId, .AddScheduleCount = 1, .Reset = false };
		nosEngine.ScheduleNode(&params);
	}

	// Delta Seconds reaches the engine via GetScheduleInfo, read only at path compile
	// time, so recompile to apply the new rate now instead of when the path next
	// rebuilds. Recompile updates the runner's timing in place but keeps its queued
	// count, so also clear it (Reset) and re-queue one to keep driving.
	void ResetSchedule()
	{
		HasDeadline = false;
		nosEngine.RecompilePath(NodeId);
		nosScheduleNodeParams params{ .NodeId = NodeId, .AddScheduleCount = 1, .Reset = true };
		nosEngine.ScheduleNode(&params);
	}

	nosResult ExecuteNode(nosNodeExecuteParams* params) override
	{
		if (Mode == ScheduleRequestMode::FixedStep)
			PaceFixedStep();
		ScheduleOnce();
		return NOS_RESULT_SUCCESS;
	}

	void OnPathStart() override
	{
		// Re-anchor the FixedStep schedule on each path start.
		HasDeadline = false;
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
