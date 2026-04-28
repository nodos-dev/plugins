// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#pragma once

#include <Nodos/PluginHelpers.hpp>

// External
#include <glm/glm.hpp>
#include <nosVulkanSubsystem/Helpers.hpp>

#include "Ring.h"
#include "nosUtil/Stopwatch.hpp"

namespace nos::utilities
{

struct MultiRingBufferNodeContext : NodeContext
{
	using RingMode = RingNodeBase::RingMode;
	using OnRestartType = RingNodeBase::OnRestartType;

	static constexpr std::string_view CHANNEL_LETTERS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

	enum MenuCommandType : uint8_t
	{
		ADD_CHANNEL = 0,
		REMOVE_CHANNEL = 1,
	};

	struct MenuCommand
	{
		MenuCommandType Type;
		uint8_t Letter;
		MenuCommand(uint32_t cmd)
		{
			Type = static_cast<MenuCommandType>(cmd & 0xFF);
			Letter = static_cast<uint8_t>((cmd >> 8) & 0xFF);
		}
		MenuCommand(MenuCommandType type, uint8_t letter) : Type(type), Letter(letter) {}
		operator uint32_t() const { return (Letter << 8) | Type; }
	};

	struct Channel
	{
		char Letter;
		nos::Name InputName;
		nos::Name OutputName;
		uuid InputId{};
		uuid OutputId{};
		nos::TypeInfo TypeInfo;
		std::unique_ptr<TRing> Ring;
		std::atomic_bool IsOutLive = false;
		ResourceInterface::ResourceBase* LastPopped = nullptr;
		bool NeedsRecreation = false;
		std::size_t RemainingRepeatableCount = 0;

		Channel(char letter)
			: Letter(letter),
			  InputName((std::string("Input_") + letter).c_str()),
			  OutputName((std::string("Output_") + letter).c_str()),
			  TypeInfo(NSN_Generic)
		{
		}
	};

	std::map<char, std::unique_ptr<Channel>> Channels;
	std::unordered_map<uuid, char> PinIdToLetter;

	OnRestartType OnRestart = OnRestartType::WAIT_UNTIL_FULL;
	std::optional<uint32_t> RequestedRingSize = std::nullopt;
	std::atomic<RingMode> Mode = RingMode::CONSUME;
	std::condition_variable ModeCV;
	std::mutex ModeMutex;
	std::atomic_bool RepeatWhenFilling = false;

	std::string GetName() const { return "MultiRingBuffer"; }

	static std::optional<char> ParseLetter(std::string_view pinName)
	{
		auto pos = pinName.find_last_of('_');
		if (pos == std::string::npos || pos + 2 != pinName.size())
			return std::nullopt;
		char c = pinName[pos + 1];
		if (c < 'A' || c > 'Z')
			return std::nullopt;
		return c;
	}

	static bool IsInputPin(std::string_view pinName) { return pinName.starts_with("Input_"); }
	static bool IsOutputPin(std::string_view pinName) { return pinName.starts_with("Output_"); }

	MultiRingBufferNodeContext(nosFbNodePtr node) : NodeContext(node)
	{
		std::vector<uuid> pinsToUnorphan;
		for (auto* pin : *node->pins())
		{
			auto pinNameSv = pin->name()->string_view();
			if (!IsInputPin(pinNameSv) && !IsOutputPin(pinNameSv))
				continue;
			auto letter = ParseLetter(pinNameSv);
			if (!letter)
				continue;

			auto& channel = Channels[*letter];
			if (!channel)
				channel = std::make_unique<Channel>(*letter);

			if (IsInputPin(pinNameSv))
				channel->InputId = uuid(*pin->id());
			else
			{
				channel->OutputId = uuid(*pin->id());
				channel->IsOutLive = pin->live();
			}
			PinIdToLetter[uuid(*pin->id())] = *letter;

			nos::Name typeName(pin->type_name()->c_str());
			if (typeName != NSN_Generic && channel->TypeInfo->TypeName == NSN_Generic)
				channel->TypeInfo = nos::TypeInfo(typeName);

			if (auto orphanState = pin->orphan_state())
				if (orphanState->type() == fb::PinOrphanStateType::ORPHAN)
					pinsToUnorphan.push_back(uuid(*pin->id()));
		}

		for (auto& [_, ch] : Channels)
			InitChannel(*ch);

		for (auto const& pinId : pinsToUnorphan)
			SetPinOrphanState(pinId, fb::PinOrphanStateType::ACTIVE);

		AddPinValueWatcher(NSN_Size, [this](nos::Buffer const& newSize, std::optional<nos::Buffer> oldVal) {
			uint32_t size = *newSize.As<uint32_t>();
			if (oldVal && oldVal == newSize)
				return;
			RequestRingResize(size);
		});
		AddPinValueWatcher(NSN_Alignment, [this](nos::Buffer const& newAlignment, std::optional<nos::Buffer> oldVal) {
			for (auto& [_, ch] : Channels)
			{
				if (!ch->Ring)
					continue;
				if (ch->Ring->ResInterface->CheckNewResource(NSN_Alignment, newAlignment, oldVal))
				{
					nosEngine.SendPathRestart(ch->InputId);
					ch->Ring->Stop();
					ch->NeedsRecreation = true;
				}
			}
		});
		AddPinValueWatcher(NOS_NAME_STATIC("RepeatWhenFilling"),
						   [this](nos::Buffer const& newVal, std::optional<nos::Buffer> oldVal) {
							   RepeatWhenFilling = *newVal.As<bool>();
						   });
	}

	~MultiRingBufferNodeContext() override
	{
		for (auto& [_, ch] : Channels)
		{
			NOS_SOFT_CHECK(ch->LastPopped == nullptr);
			if (ch->Ring)
				ch->Ring->Stop();
		}
	}

	void InitChannel(Channel& ch)
	{
		std::shared_ptr<ResourceInterface> resource;
		if (ch.TypeInfo->TypeName == NOS_NAME(sys::vulkan::Buffer::GetFullyQualifiedName()))
			resource = std::make_shared<GPUBufferResource>();
		else if (ch.TypeInfo->TypeName == NOS_NAME(sys::vulkan::Texture::GetFullyQualifiedName()))
			resource = std::make_shared<GPUTextureResource>();
		else
			resource = std::make_shared<CPUTrivialResource>();

		ch.Ring = std::make_unique<TRing>(1, std::move(resource));
		ch.Ring->Stop();
	}

	Channel* GetChannelByPinId(uuid const& id)
	{
		auto it = PinIdToLetter.find(id);
		if (it == PinIdToLetter.end())
			return nullptr;
		auto chIt = Channels.find(it->second);
		return chIt != Channels.end() ? chIt->second.get() : nullptr;
	}

	void SeedOutputPin(Channel& ch)
	{
		if (!ch.Ring || !ch.Ring->IsResourcesValid())
			return;
		auto* base = ch.Ring->Resources[0].get();
		if (!base)
			return;
		if (ch.TypeInfo->TypeName == NOS_NAME(sys::vulkan::Buffer::GetFullyQualifiedName()))
		{
			if (auto* res = ResourceInterface::GetResource<GPUBufferResource>(base))
				nosEngine.SetPinValueByName(NodeId, ch.OutputName, res->VkRes.ToPinData());
		}
		else if (ch.TypeInfo->TypeName == NOS_NAME(sys::vulkan::Texture::GetFullyQualifiedName()))
		{
			if (auto* res = ResourceInterface::GetResource<GPUTextureResource>(base))
			{
				sys::vulkan::TTexture texDef = vkss::ConvertTextureInfo(res->VkRes);
				texDef.unscaled = true;
				nosEngine.SetPinValueByName(NodeId, ch.OutputName, nos::Buffer::From(texDef));
			}
		}
	}

	void RequestRingResize(uint32_t size)
	{
		if (size == 0)
		{
			nosEngine.LogW((GetName() + " size cannot be 0").c_str());
			return;
		}
		bool changed = false;
		for (auto& [_, ch] : Channels)
		{
			if (!ch->Ring)
				continue;
			if (ch->Ring->Size != size && (!RequestedRingSize.has_value() || *RequestedRingSize != size))
			{
				nosPathCommand ringSizeChange{.Event = NOS_RING_SIZE_CHANGE, .RingSize = size};
				nosEngine.SendPathCommand(ch->InputId, ringSizeChange);
				ch->Ring->Stop();
				changed = true;
			}
		}
		if (changed)
		{
			SendPathRestart();
			RequestedRingSize = size;
		}
	}

	void SendPathRestart()
	{
		for (auto& [_, ch] : Channels)
			nosEngine.SendPathRestart(ch->InputId);
	}

	void OnPinValueChanged(nos::Name pinName, uuid const& pinId, nosBuffer value) override
	{
		auto sv = pinName.AsString();
		if (!IsInputPin(sv))
			return;
		auto* ch = GetChannelByPinId(pinId);
		if (!ch || !ch->Ring)
			return;
		if (ch->Ring->ResInterface->CheckNewResource(NSN_Input, value, std::nullopt))
		{
			nosEngine.SendPathRestart(ch->InputId);
			ch->Ring->Stop();
			ch->NeedsRecreation = true;
		}
	}

	nosResult OnResolvePinDataTypes(nosResolvePinDataTypesParams* params) override
	{
		auto pinNameStr = nos::Name(params->InstigatorPinName).AsString();
		auto letter = ParseLetter(pinNameStr);
		if (!letter)
			return NOS_RESULT_FAILED;
		auto chIt = Channels.find(*letter);
		if (chIt == Channels.end())
			return NOS_RESULT_FAILED;
		auto& ch = *chIt->second;
		if (ch.TypeInfo->TypeName != NSN_Generic)
			return NOS_RESULT_FAILED;
		ch.TypeInfo = nos::TypeInfo(params->IncomingTypeName);
		// Drop the Generic-fallback ring so OnPinUpdated re-inits with the resolved type.
		if (ch.Ring)
			ch.Ring->Stop();
		ch.Ring.reset();
		for (size_t i = 0; i < params->PinCount; i++)
		{
			auto& pinInfo = params->Pins[i];
			if (pinInfo.Id == ch.InputId || pinInfo.Id == ch.OutputId)
				pinInfo.OutResolvedTypeName = ch.TypeInfo->TypeName;
		}
		return NOS_RESULT_SUCCESS;
	}

	void OnPinUpdated(const nosPinUpdate*) override
	{
		for (auto& [_, ch] : Channels)
			if (!ch->Ring)
				InitChannel(*ch);
	}

	void OnNodeUpdated(nosNodeUpdate const* update) override
	{
		if (update->Type == NOS_NODE_UPDATE_PIN_DELETED)
		{
			auto it = PinIdToLetter.find(update->PinDeleted);
			if (it == PinIdToLetter.end())
				return;
			char letter = it->second;
			PinIdToLetter.erase(it);
			auto chIt = Channels.find(letter);
			if (chIt == Channels.end())
				return;
			auto& ch = *chIt->second;
			bool inputAlive = PinIdToLetter.contains(ch.InputId);
			bool outputAlive = PinIdToLetter.contains(ch.OutputId);
			if (!inputAlive && !outputAlive)
			{
				if (ch.Ring)
					ch.Ring->Stop();
				Channels.erase(chIt);
			}
		}
		else if (update->Type == NOS_NODE_UPDATE_PIN_CREATED)
		{
			auto* pin = update->PinCreated;
			auto sv = pin->name()->string_view();
			if (!IsInputPin(sv) && !IsOutputPin(sv))
				return;
			auto letter = ParseLetter(sv);
			if (!letter)
				return;
			auto& chPtr = Channels[*letter];
			if (!chPtr)
				chPtr = std::make_unique<Channel>(*letter);
			if (IsInputPin(sv))
				chPtr->InputId = uuid(*pin->id());
			else
			{
				chPtr->OutputId = uuid(*pin->id());
				chPtr->IsOutLive = pin->live();
			}
			PinIdToLetter[uuid(*pin->id())] = *letter;
			if (!chPtr->Ring)
				InitChannel(*chPtr);
		}
	}

	nosResult ExecuteNode(nosNodeExecuteParams* params) override
	{
		if (Channels.empty())
			return NOS_RESULT_FAILED;
		NodeExecuteParams pins(params);
		uint32_t requestedSize = *pins.GetPinData<uint32_t>(NSN_Size);

		std::vector<std::pair<Channel*, void*>> inputs;
		inputs.reserve(Channels.size());
		uint32_t maxRequired = requestedSize;
		std::string adjustMessage;
		for (auto& [letter, ch] : Channels)
		{
			// Skip channels whose ring isn't ready yet (e.g. just added,
			// awaiting OnNodeUpdated/OnPinUpdated to finish init).
			if (!ch->Ring || ch->Ring->Exit || !ch->Ring->IsResourcesValid() || !ch->TypeInfo)
				continue;
			auto it = pins.find(ch->InputName);
			if (it == pins.end())
				continue;
			void* input = ch->Ring->ResInterface->GetPinInfo(it->second, true);
			if (!input)
				continue;
			auto [required, message] = ch->Ring->ResInterface->GetRequiredRingSize(input, requestedSize);
			if (required > maxRequired)
			{
				maxRequired = required;
				adjustMessage = message;
			}
			inputs.emplace_back(ch.get(), input);
		}
		if (inputs.empty())
		{
			SendScheduleRequest(0);
			return NOS_RESULT_FAILED;
		}

		bool effectiveSizeAdjusted = maxRequired != requestedSize;
		ClearNodeStatusMessages();
		if (effectiveSizeAdjusted)
			SetNodeStatusMessage(adjustMessage, fb::NodeStatusMessageType::WARNING);

		bool anyResize = false;
		for (auto& [ch, _] : inputs)
			if (ch->Ring->Size != maxRequired)
			{
				anyResize = true;
				break;
			}

		if (anyResize)
		{
			RequestRingResize(maxRequired);
			if (effectiveSizeAdjusted)
				nosEngine.LogW("%s", adjustMessage.c_str());
			return NOS_RESULT_FAILED;
		}

		std::vector<ResourceInterface::ResourceBase*> slots(inputs.size(), nullptr);
		for (size_t i = 0; i < inputs.size(); ++i)
		{
			auto* ch = inputs[i].first;
			auto* slot = ch->Ring->BeginPush(100);
			if (!slot)
			{
				for (size_t j = 0; j < i; ++j)
					inputs[j].first->Ring->CancelPush(slots[j]);
				if (ch->Ring->Exit)
					return NOS_RESULT_FAILED;
				return NOS_RESULT_PENDING;
			}
			slots[i] = slot;
		}

		for (size_t i = 0; i < inputs.size(); ++i)
		{
			auto* ch = inputs[i].first;
			ch->Ring->ResInterface->Push(slots[i], inputs[i].second, params,
										 NOS_NAME_STATIC("MultiRingBuffer"), true);
			ch->Ring->EndPush(slots[i]);
			if (!ch->IsOutLive)
			{
				ChangePinLiveness(ch->OutputName, true);
				ch->IsOutLive = true;
			}
		}

		if (Mode == RingMode::FILL)
		{
			bool isFillComplete = true;
			for (auto& [ch, _] : inputs)
				if (ch->Ring->Write.Pool.size() != 0)
				{
					isFillComplete = false;
					break;
				}
			if (isFillComplete)
			{
				Mode = RingMode::CONSUME;
				ModeCV.notify_all();
			}
		}

		return NOS_RESULT_SUCCESS;
	}

	nosResult CopyFrom(nosCopyInfo* cpy) override
	{
		auto* ch = GetChannelByPinId(cpy->ID);
		if (!ch || !ch->Ring || ch->Ring->Exit)
			return NOS_RESULT_FAILED;
		if (!ch->IsOutLive)
			return NOS_RESULT_SUCCESS;

		// EndPop the previous frame's slot before popping a new one. We can't
		// rely on OnEndFrame: the engine only fires it on the path's primary
		// source pin, so live secondary outputs (e.g. a second channel feeding
		// the same consumer) never receive it. By the time the consumer asks
		// for the next frame on this pin, it's done with the previous one.
		if (ch->LastPopped)
		{
			ch->Ring->EndPop(ch->LastPopped);
			ch->LastPopped = nullptr;
		}

		if (OnRestart == OnRestartType::WAIT_UNTIL_FULL && RepeatWhenFilling)
		{
			if (ch->RemainingRepeatableCount > 0)
			{
				ch->Ring->ResInterface->OnRepeatPinValue(cpy);
				ch->RemainingRepeatableCount--;
				return NOS_RESULT_SUCCESS;
			}
		}
		else if (Mode == RingMode::FILL)
		{
			std::unique_lock lock(ModeMutex);
			if (!ModeCV.wait_for(lock, std::chrono::milliseconds(100),
								 [this] { return Mode != RingMode::FILL; }))
				return NOS_RESULT_PENDING;
		}

		ResourceInterface::ResourceBase* slot;
		{
			ScopedProfilerEvent _({.Name = "Wait For Filled Slot"});
			slot = ch->Ring->BeginPop(100);
		}
		if (!slot)
			return ch->Ring->Exit ? NOS_RESULT_FAILED : NOS_RESULT_PENDING;

		nos::Buffer outPinVal;
		bool changePinValue = ch->Ring->ResInterface->BeginCopyFrom(slot, *cpy->PinData, outPinVal);
		if (changePinValue)
			nosEngine.SetPinValueByName(NodeId, ch->OutputName, outPinVal);

		ch->Ring->ResInterface->WaitForDownloadToEnd(slot, "MultiRingBuffer", NodeName.AsString(), cpy);

		cpy->CopyFromOptions.ShouldSetSourceFrameNumber = true;
		cpy->FrameNumber = slot->FrameNumber;

		ch->LastPopped = slot;
		SendScheduleRequest(1);
		return NOS_RESULT_SUCCESS;
	}

	void OnEndFrame(uuid const& pinId, nosEndFrameCause cause) override
	{
		auto* ch = GetChannelByPinId(pinId);
		if (!ch)
			return;

		if (cause == NOS_END_FRAME_FAILED)
		{
			if (pinId == ch->OutputId)
				return;
			if (!ch->IsOutLive)
				return;
			ChangePinLiveness(ch->OutputName, false);
			ch->IsOutLive = false;
		}
		// Note: EndPop happens at the start of the next CopyFrom for this
		// channel rather than here, because OnEndFrame is unreliable for
		// secondary live outputs.
	}

	void SendScheduleRequest(uint32_t count, bool reset = false) const
	{
		nosScheduleNodeParams schedule{.NodeId = NodeId, .AddScheduleCount = count, .Reset = reset};
		nosEngine.ScheduleNode(&schedule);
	}

	void OnPathCommand(const nosPathCommand* command) override
	{
		switch (command->Event)
		{
		case NOS_RING_SIZE_CHANGE:
			if (command->RingSize == 0)
				return;
			RequestedRingSize = command->RingSize;
			nosEngine.SetPinValue(*GetPinId(NSN_Size), nos::Buffer::From(command->RingSize));
			break;
		default: return;
		}
	}

	void OnPathStop() override
	{
		if (OnRestart == OnRestartType::WAIT_UNTIL_FULL)
			Mode = RingMode::FILL;
		for (auto& [_, ch] : Channels)
		{
			if (ch->LastPopped && ch->Ring)
			{
				ch->Ring->EndPop(ch->LastPopped);
				ch->LastPopped = nullptr;
			}
			if (ch->Ring)
				ch->Ring->Stop();
		}
	}

	void OnPathStart() override
	{
		if (Channels.empty())
			return;
		size_t totalSchedule = 0;
		for (auto& [_, ch] : Channels)
		{
			if (!ch->Ring)
				continue;
			if (OnRestart == OnRestartType::RESET || RepeatWhenFilling)
				ch->Ring->Reset(false);
			else if (ch->Ring->IsFull() && !ch->Ring->Read.Pool.empty())
			{
				ch->Ring->Write.Pool.push_back(ch->Ring->Read.Pool.front());
				ch->Ring->Read.Pool.pop_front();
			}
			if (RequestedRingSize)
			{
				ch->Ring->Resize(*RequestedRingSize);
				ch->NeedsRecreation = false;
			}
			if (ch->NeedsRecreation)
			{
				ch->Ring = std::make_unique<TRing>(ch->Ring->Size, ch->Ring->ResInterface);
				ch->NeedsRecreation = false;
			}
			if (!ch->Ring->IsResourcesValid())
			{
				totalSchedule = std::max<size_t>(totalSchedule, 1);
				continue;
			}
			auto emptySlotCount = ch->Ring->Write.Pool.size();
			if (RepeatWhenFilling)
				ch->RemainingRepeatableCount = std::max(emptySlotCount, (size_t)1) - 1;
			totalSchedule = std::max(totalSchedule, emptySlotCount);
			ch->Ring->Exit = false;
			ch->Ring->ResInterface->OnPathStart();
			SeedOutputPin(*ch);
		}
		RequestedRingSize = std::nullopt;
		if (totalSchedule > 0)
		{
			nosScheduleNodeParams schedule{.NodeId = NodeId, .AddScheduleCount = (uint32_t)totalSchedule};
			nosEngine.ScheduleNode(&schedule);
		}
	}

	void OnNodeMenuRequested(nosContextMenuRequestPtr request) override
	{
		flatbuffers::FlatBufferBuilder fbb;
		std::vector items = {
			nos::CreateContextMenuItemDirect(fbb, "Add Channel", MenuCommand(ADD_CHANNEL, 0))};
		HandleEvent(CreateAppEvent(fbb, app::CreateAppContextMenuUpdateDirect(
											fbb, request->item_id(), request->pos(), request->instigator(), &items)));
	}

	void OnPinMenuRequested(nos::Name pinName, nosContextMenuRequestPtr request) override
	{
		auto sv = pinName.AsString();
		if (!IsInputPin(sv) && !IsOutputPin(sv))
			return;
		auto letter = ParseLetter(sv);
		if (!letter)
			return;
		if (Channels.size() <= 1)
			return;
		flatbuffers::FlatBufferBuilder fbb;
		std::vector items = {nos::CreateContextMenuItemDirect(
			fbb, "Remove Channel", MenuCommand(REMOVE_CHANNEL, static_cast<uint8_t>(*letter)))};
		HandleEvent(CreateAppEvent(fbb, app::CreateAppContextMenuUpdateDirect(
											fbb, request->item_id(), request->pos(), request->instigator(), &items)));
	}

	void OnMenuCommand(uuid const& itemID, uint32_t cmd) override
	{
		auto command = MenuCommand(cmd);
		switch (command.Type)
		{
		case ADD_CHANNEL:
		{
			char newLetter = 0;
			for (char c : CHANNEL_LETTERS)
			{
				if (!Channels.contains(c))
				{
					newLetter = c;
					break;
				}
			}
			if (newLetter == 0)
			{
				SetNodeStatusMessage("Maximum number of channels reached", fb::NodeStatusMessageType::WARNING);
				return;
			}

			fb::TPin inPin;
			inPin.id = uuid(nosEngine.GenerateID());
			inPin.name = std::string("Input_") + newLetter;
			inPin.type_name = "nos.Generic";
			inPin.show_as = fb::ShowAs::INPUT_PIN;
			inPin.can_show_as = fb::CanShowAs::INPUT_PIN_ONLY;

			fb::TPin outPin;
			outPin.id = uuid(nosEngine.GenerateID());
			outPin.name = std::string("Output_") + newLetter;
			outPin.type_name = "nos.Generic";
			outPin.show_as = fb::ShowAs::OUTPUT_PIN;
			outPin.can_show_as = fb::CanShowAs::OUTPUT_PIN_ONLY;
			outPin.live = true;

			nos::TPartialNodeUpdate update;
			update.node_id = NodeId;
			update.pins_to_add.emplace_back(std::make_unique<fb::TPin>(std::move(inPin)));
			update.pins_to_add.emplace_back(std::make_unique<fb::TPin>(std::move(outPin)));
			flatbuffers::FlatBufferBuilder fbb;
			HandleEvent(CreateAppEvent(fbb, nos::CreatePartialNodeUpdate(fbb, &update)));
			break;
		}
		case REMOVE_CHANNEL:
		{
			char letter = static_cast<char>(command.Letter);
			auto it = Channels.find(letter);
			if (it == Channels.end())
				return;
			auto& ch = *it->second;
			nos::TPartialNodeUpdate update;
			update.node_id = NodeId;
			update.pins_to_delete = {ch.InputId, ch.OutputId};
			flatbuffers::FlatBufferBuilder fbb;
			HandleEvent(CreateAppEvent(fbb, nos::CreatePartialNodeUpdate(fbb, &update)));
			break;
		}
		}
	}
};

nosResult RegisterMultiRingBuffer(nosNodeFunctions* functions)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("MultiRingBuffer"), MultiRingBufferNodeContext, functions)
	return NOS_RESULT_SUCCESS;
}

} // namespace nos::utilities
