// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/PluginHelpers.hpp>
#include <nosVulkanSubsystem/nosVulkanSubsystem.h>

// stl
#include <chrono>
#include <list>
#include <nosUtil/Stopwatch.hpp>

#include "Sink_generated.h"

namespace nos::utilities
{
using clock = std::chrono::high_resolution_clock;

constexpr uint64_t VULKAN_TIMEOUT_BEFORE_LEAK =
	3'000'000'000; // 3 seconds. This exists in case a GPU event is never signaled, we prefer to leak than hang forever.

struct SinkNode : NodeContext
{
	enum MenuCommandType : uint8_t
	{
		ADD_INPUT = 0,
		REMOVE_INPUT = 1,
	};

	struct MenuCommand
	{
		MenuCommandType Type;
		uint8_t InputIndex;
		MenuCommand(uint32_t cmd) {
			Type = static_cast<MenuCommandType>(cmd & 0xFF);
			InputIndex = static_cast<uint8_t>((cmd >> 8) & 0xFF);
		}
		MenuCommand(MenuCommandType type, uint8_t inputIndex) : Type(type), InputIndex(inputIndex) {}
		operator uint32_t() const { return (InputIndex << 8) | Type; }
	};

	static const std::unordered_set<std::string_view>& StaticPinNames()
	{
		static const std::unordered_set<std::string_view> names = {
			"InExe", "Sink Input", "Sink FPS", "HasGPUWork", "GPUFrameBuffering",
			"AcceptsRepeat", "SinkMode", "LatencyBudget"
		};
		return names;
	}

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
	std::optional<std::vector<nosGPUEvent>> GPUFrameSyncEvents = std::nullopt;
	size_t GPUFrameBuffering = 1;
	uint64_t CurrentGPUEventIndex = 0;
	std::vector<uuid> DynamicInputs;


	SinkNode(nosFbNodePtr inNode) : NodeContext(inNode) {
		std::list<uuid> pinsToUnorphan;
		for (auto i = 0; i < inNode->pins()->size(); i++)
		{
			auto pin = inNode->pins()->Get(i);
			if (pin->show_as() != fb::ShowAs::INPUT_PIN)
				continue;
			if (StaticPinNames().contains(pin->name()->string_view()))
				continue;
			DynamicInputs.push_back(*pin->id());
			if (auto orphanState = pin->orphan_state())
			{
				if (orphanState->type() == fb::PinOrphanStateType::ORPHAN)
					pinsToUnorphan.push_back(*pin->id());
			}
		}
		for (auto const& pinId : pinsToUnorphan)
			SetPinOrphanState(pinId, fb::PinOrphanStateType::ACTIVE);
		AddPinValueWatcher(NOS_NAME("HasGPUWork"),
						   [this](nosBuffer const& newVal, std::optional<nos::Buffer> oldValue) {
							   bool hasGpuWork = *static_cast<bool*>(newVal.Data);
				SetPinOrphanState(NOS_NAME("GPUFrameBuffering"),
								  hasGpuWork ? fb::PinOrphanStateType::ACTIVE : fb::PinOrphanStateType::ORPHAN,
								  hasGpuWork ? nullptr : "Sink does not have GPU work to buffer.");
							   if (hasGpuWork == GPUFrameSyncEvents.has_value())
								   return;
							   if (!hasGpuWork)
							   {
								   // Clear GPU events
								   for (auto& event : *GPUFrameSyncEvents)
									   DestroyGPUEvent(event);
								   GPUFrameSyncEvents = std::nullopt;
							   }
							   else
							   {
								   GPUFrameSyncEvents = std::vector<nosGPUEvent>(GPUFrameBuffering);
								   CurrentGPUEventIndex = 0;
							   }
						   });
		AddPinValueWatcher(NOS_NAME("GPUFrameBuffering"), [this](nosBuffer const& newVal, auto&&) {
			size_t newBufferSize = *static_cast<size_t*>(newVal.Data);
			if (newBufferSize == 0)
				newBufferSize = 1;
			GPUFrameBuffering = newBufferSize;
			if (!GPUFrameSyncEvents.has_value())
				return;
			if (GPUFrameBuffering != GPUFrameSyncEvents->size())
			{
				for (auto& event : *GPUFrameSyncEvents)
					DestroyGPUEvent(event);
				GPUFrameSyncEvents->resize(GPUFrameBuffering);
				CurrentGPUEventIndex = 0;
			}
		});
	}

	~SinkNode() override
	{
		StopThread();
	}

	void DestroyGPUEvent(nosGPUEvent& event)
	{
		if (event)
		{
			if (nosVulkan->WaitGpuEvent(&event, VULKAN_TIMEOUT_BEFORE_LEAK) != NOS_RESULT_SUCCESS)
			{
				nosEngine.LogW("Sink Node timed out waiting for GPU event during destruction, leaking the event to avoid hang.");
			}
			event = {};
		}
	}

	nosResult ExecuteNode(nosNodeExecuteParams* params) override
	{
		if (nosVulkan && GPUFrameSyncEvents)
		{
			auto& gpuEvent = (*GPUFrameSyncEvents)[CurrentGPUEventIndex];
			if (gpuEvent)
			{
				if (nosVulkan->WaitGpuEvent(&gpuEvent, 100'000'000) != NOS_RESULT_SUCCESS)
					return NOS_RESULT_PENDING;
				gpuEvent = {};
			}
			nosCmd cmd{};
			nosCmdBeginParams beginParams = {.Name = NOS_NAME("Sink Submit"), .AssociatedNodeId = NodeId, .OutCmdHandle = &cmd};
			nosVulkan->Begin(&beginParams);
			nosCmdEndParams endParams{.ForceSubmit = true, .OutGPUEventHandle = &gpuEvent};
			nosVulkan->End(cmd, &endParams);
			CurrentGPUEventIndex = (CurrentGPUEventIndex + 1) % GPUFrameSyncEvents->size();
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
		if (GPUFrameSyncEvents)
		{
			// Wait all GPU events
			for (auto& event : *GPUFrameSyncEvents)
			{
				DestroyGPUEvent(event);
			}
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

	void OnNodeMenuRequested(nosContextMenuRequestPtr request) override
	{
		uint32_t cmd = MenuCommand(ADD_INPUT, 0);
		flatbuffers::FlatBufferBuilder fbb;
		std::vector items = {
			nos::CreateContextMenuItemDirect(fbb, "Add Sink", cmd, nullptr)
		};
		HandleEvent(CreateAppEvent(fbb, app::CreateAppContextMenuUpdateDirect(
			fbb, request->item_id(), request->pos(), request->instigator(), &items)));
	}

	void OnPinMenuRequested(nos::Name pinName, nosContextMenuRequestPtr request) override
	{
		if (StaticPinNames().contains(pinName.AsString()))
			return;
		auto pinId = GetPinId(pinName);
		if (!pinId)
			return;
		auto it = std::find(DynamicInputs.begin(), DynamicInputs.end(), *pinId);
		if (it == DynamicInputs.end())
			return;
		auto index = std::distance(DynamicInputs.begin(), it);
		uint32_t cmd = MenuCommand(REMOVE_INPUT, static_cast<uint8_t>(index));
		flatbuffers::FlatBufferBuilder fbb;
		std::vector items = {
			nos::CreateContextMenuItemDirect(fbb, "Remove Input", cmd, nullptr)
		};
		HandleEvent(CreateAppEvent(fbb, app::CreateAppContextMenuUpdateDirect(
			fbb, request->item_id(), request->pos(), request->instigator(), &items)));
	}

	void OnMenuCommand(uuid const& itemID, uint32_t cmd) override
	{
		auto command = MenuCommand(cmd);
		switch (command.Type)
		{
		case ADD_INPUT:
		{
			std::string pinName;
			for (size_t i = 2;; i++)
			{
				auto candidate = "Sink Input " + std::to_string(i);
				if (!GetPinId(nos::Name(candidate)))
				{
					pinName = std::move(candidate);
					break;
				}
			}
			flatbuffers::FlatBufferBuilder fbb;
			uuid pinId = nosEngine.GenerateID();
			std::vector pins = {
				fb::CreatePinDirect(fbb, &pinId, pinName.c_str(), "nos.Generic",
					fb::ShowAs::INPUT_PIN, fb::CanShowAs::INPUT_PIN_ONLY)
			};
			HandleEvent(CreateAppEvent(fbb, CreatePartialNodeUpdateDirect(fbb, &NodeId, ClearFlags::NONE, 0, &pins)));
			break;
		}
		case REMOVE_INPUT:
		{
			if (command.InputIndex >= DynamicInputs.size())
				return;
			auto pinId = DynamicInputs[command.InputIndex];
			flatbuffers::FlatBufferBuilder fbb;
			std::vector pinsToRemove = { *&pinId };
			HandleEvent(CreateAppEvent(fbb, CreatePartialNodeUpdateDirect(fbb, &NodeId, ClearFlags::NONE, &pinsToRemove)));
			break;
		}
		}
	}

	void OnNodeUpdated(nosNodeUpdate const* update) override
	{
		if (update->Type == NOS_NODE_UPDATE_PIN_DELETED)
		{
			std::erase_if(DynamicInputs, [&](auto id) { return id == update->PinDeleted; });
		}
		else if (update->Type == NOS_NODE_UPDATE_PIN_CREATED)
		{
			auto* pin = update->PinCreated;
			if (pin->show_as() != fb::ShowAs::INPUT_PIN)
				return;
			if (StaticPinNames().contains(pin->name()->string_view()))
				return;
			DynamicInputs.push_back(*pin->id());
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
