// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/PluginHelpers.hpp>
#include <nosVulkanSubsystem/nosVulkanSubsystem.h>

// stl
#include <chrono>
#include <nosUtil/Stopwatch.hpp>

#include "Sink_generated.h"

namespace nos::utilities
{
using clock = std::chrono::high_resolution_clock;

struct SinkNode : NodeContext
{
	std::mutex Mutex;
	std::atomic<bool> ShouldStop = false;
	std::atomic<float> Fps = 1000.0f / 60.0f;
	std::atomic<bool> Wait = true;
	std::thread Thread;
	bool AcceptRepeat = false;
	utilities::SinkMode Mode = utilities::SinkMode::Periodic;
	std::atomic<int64_t> PendingRequests = 0;
	std::atomic<float> LatencyBudget = 1.0f;
	bool HasDroppingMessage = false;
	std::vector<nosGPUEvent> GPUFrameSyncEvents;
	uint64_t CurrentGPUEventIndex = 0;


	SinkNode(nosFbNodePtr inNode) : NodeContext(inNode)
	{
	}

	~SinkNode() override
	{
		StopThread();
	}

	nosResult ExecuteNode(nosNodeExecuteParams* params) override
	{
		if (nosVulkan)
		{
			auto& gpuEvent = GPUFrameSyncEvents[CurrentGPUEventIndex];
			nosCmd cmd{};
			nosCmdBeginParams beginParams = {.Name = NOS_NAME("Sink Submit"), .AssociatedNodeId = NodeId, .OutCmdHandle = &cmd};
			nosVulkan->Begin(&beginParams);
			nosCmdEndParams endParams{.ForceSubmit = true, .OutGPUEventHandle = &gpuEvent};
			nosVulkan->End(cmd, &endParams);
			uint64_t nextGPUEventIndex = (CurrentGPUEventIndex + 1) % GPUFrameSyncEvents.size();
			auto& nextGpuEvent = GPUFrameSyncEvents[nextGPUEventIndex];
			if (nextGpuEvent)
			{
				nosVulkan->WaitGpuEvent(&nextGpuEvent, UINT64_MAX);
				nextGpuEvent = {};
			}
			CurrentGPUEventIndex = nextGPUEventIndex;
		}

		if (!IsPeriodic())
			ScheduleNode();
		else
			PendingRequests--;

		return NOS_RESULT_SUCCESS;
	}

	void OnPinValueChanged(nos::Name pinName, uuid const& pinId, nosBuffer value) override
	{
		if (NOS_NAME("Sink FPS") == pinName)
		{
			Fps = *static_cast<float*>(value.Data);
			nosEngine.RecompilePath(NodeId);
		}
		if (NOS_NAME("Wait") == pinName)
		{
			Wait = *static_cast<bool*>(value.Data);
		}
		if (NOS_NAME("AcceptRepeat") == pinName)
		{
			AcceptRepeat = *static_cast<bool*>(value.Data);
			nosEngine.RecompilePath(NodeId);
		}
		if (NOS_NAME("SinkMode") == pinName)
		{
			SetMode(*static_cast<utilities::SinkMode*>(value.Data));
		}
		if (NOS_NAME("LatencyBudget") == pinName)
		{
			LatencyBudget = *static_cast<float*>(value.Data);
		}
		if (NOS_NAME("GPUFrameBuffering") == pinName)
		{
			size_t newBufferSize = *static_cast<size_t*>(value.Data);
			if (newBufferSize == 0)
				newBufferSize = 1;
			if (newBufferSize != GPUFrameSyncEvents.size())
			{
				for (auto& event : GPUFrameSyncEvents)
				{
					if (event)
						nosVulkan->WaitGpuEvent(&event, 1000000000);
					event = {};
				}
				GPUFrameSyncEvents.resize(newBufferSize);
				CurrentGPUEventIndex = 0;
			}
		}
	}

	void SetMode(utilities::SinkMode mode)
	{
		Mode = mode;
		nosEngine.RecompilePath(NodeId);
		auto fpsPinId = *GetPinId(NOS_NAME("Sink FPS"));
		auto latencyBudgetPinId = *GetPinId(NOS_NAME("LatencyBudget"));
		nosOrphanState orphanState{
			.Type = !IsPeriodic() ? NOS_ORPHAN_STATE_TYPE_ORPHAN : NOS_ORPHAN_STATE_TYPE_ACTIVE,
			.Message = !IsPeriodic() ? "Periodic mode is disabled" : ""
		};
		nosEngine.SetItemOrphanState(fpsPinId, &orphanState);
		nosEngine.SetItemOrphanState(latencyBudgetPinId, &orphanState);
		ClearNodeStatusMessages();
	}

	bool IsPeriodic()
	{
		return Mode == utilities::SinkMode::Periodic;
	}

	void StopThread() {
		ShouldStop = true;
		if (Thread.joinable())
			Thread.join();
	}

	void StartThread() {
		StopThread();
		ShouldStop = false;
		Thread = std::thread([this]() { SinkThread(); });
		flatbuffers::FlatBufferBuilder fbb;
		HandleEvent(CreateAppEvent(fbb, nos::app::CreateSetThreadNameDirect(fbb, (uint64_t)Thread.native_handle(), "Sink Thread")));
	}

	void OnPathStart() override 
	{
		PendingRequests = 0;
		if (!IsPeriodic())
		{
			StopThread();
			ScheduleNode();
		}
		else if (!Thread.joinable())
			StartThread();
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

	void OnPathStop() override
	{
		if (IsPeriodic())
			StopThread();
		// Wait all GPU events
		for (auto& event : GPUFrameSyncEvents)
		{
			if (event)
				nosVulkan->WaitGpuEvent(&event, 1000000000);
			event = {};
		}
	}

	void SinkThread()
	{
		clock::time_point start = clock::now();
		clock::time_point lastSchedule;
		bool dropped = false;
		while (!ShouldStop)
		{
			auto now = clock::now();

			float diff = std::chrono::duration_cast<std::chrono::microseconds>(now - lastSchedule).count() / 1000.0f;
			if (diff < 1000.f / Fps)
				continue;
			lastSchedule = now;
			if ((float)PendingRequests.load() / Fps.load() > LatencyBudget.load())
			{
				nosEngine.LogE("Sink Node dropping frame, pending requests: %d, fps: %.2f, latency budget: %.2f",
							   (int)PendingRequests.load(),
							   Fps.load(),
							   LatencyBudget.load());
				SetNodeStatusMessage("Dropping", fb::NodeStatusMessageType::WARNING);
				nosEngine.SendPathRestart(NodeId);
				HasDroppingMessage = true;
				dropped = true;
				return;
			}
			if (HasDroppingMessage && !dropped)
			{
				if (now - start > std::chrono::milliseconds(int64_t(LatencyBudget.load() * 2.0 * 1000.0)))
				{
					ClearNodeStatusMessages();
					HasDroppingMessage = false;
				}
			}
			PendingRequests++;
			{
				std::unique_lock lock(Mutex);
				if (ShouldStop)
					break;
				ScheduleNode();
			}
		}
	}

	void GetScheduleInfo(nosScheduleInfo* info) override
	{
		info->Type = NOS_SCHEDULE_TYPE_ON_DEMAND;
		if (IsPeriodic())
			info->DeltaSeconds = {10000u, (uint32_t)std::floor(Fps * 10000)};
		else
			info->DeltaSeconds = {0, 0};
		nosEngine.LogI("Sink Node delta seconds: %d/%d", info->DeltaSeconds.x, info->DeltaSeconds.y);
		info->Importance = 0;
		for (int i = 0; i < info->PinInfosCount; i++)
			info->PinInfos[i].NeedsRepeat = AcceptRepeat;
	}

};

nosResult RegisterSink(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("Sink"), SinkNode, fn)
	return NOS_RESULT_SUCCESS;
}
}
