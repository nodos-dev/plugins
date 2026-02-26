// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include "TypeCommon.h"

#include <nosVulkanSubsystem/nosVulkanSubsystem.h>
#include <nosVulkanSubsystem/Helpers.hpp>

namespace nos::reflect
{

struct SlotBase
{
	nos::Buffer Buffer;
	virtual ~SlotBase() = default;
	SlotBase(const SlotBase&) = delete;
	SlotBase& operator=(const SlotBase&) = delete;
	SlotBase(nosBuffer const& buf) : Buffer(buf) {}
	virtual void CopyFrom(nosBuffer const& buf, uuid const& nodeId) = 0;
	virtual bool IsSlotCompatible(nosBuffer const& buf) const = 0;
	virtual uint64_t GetPhaseCount() const { return 1; }
	virtual std::optional<std::string> HasPhaseCountDelayMismatchWarning(uint64_t delay) const { return std::nullopt; }
};

struct DelayQueue {
	std::vector<std::unique_ptr<SlotBase>> Slots;
	std::queue<Ref<SlotBase>> ReadyQueue;
	// This is the actively used slot, which is the output pin's value, if AccountForActiveSlot is true.
	SlotBase* ActiveSlot{};
	std::queue<Ref<SlotBase>> FreeQueue;
	uint32_t Delay;
	bool AccountForActiveSlot = false;
	DelayQueue(uint32_t delay) : Delay(delay) {}
	DelayQueue(const DelayQueue&) = delete;
	void Clear()
	{
		ActiveSlot = nullptr;
		ReadyQueue = {};
		FreeQueue = {};
		Slots.clear(); 
	}

	void ClearIfIncompatibleData(nosBuffer const& buf)
	{
		if (AreSlotsCompatibleWith(buf))
			return;
		Clear();
	}

	bool HasFree() const 
	{
		return !FreeQueue.empty(); 
	}

	void DeleteSlotAndPopFromQueue(std::queue<Ref<SlotBase>>& queue)
	{
		if (!queue.empty())
		{
			auto slotPtr = queue.front();
			queue.pop();
			std::erase_if(Slots, [slotPtr](const std::unique_ptr<SlotBase>& slot) { return slot.get() == &slotPtr; });
		}
	}

	SlotBase* BeginPush()
	{
		if (!HasFree())
			return nullptr;
		auto slotPtr = &*FreeQueue.front();
		FreeQueue.pop();
		return slotPtr;
	}
	void EndPush(SlotBase& slot)
	{
		ReadyQueue.push(slot);
		// If there are more than Delay slots in the queue, we need to delete the oldest ones.
		while (ReadyQueue.size() > Delay)
			DeleteSlotAndPopFromQueue(ReadyQueue);
	}
	SlotBase* BeginPop()
	{
		if (ReadyQueue.size() < Delay)
			return nullptr;
		// If there are more than Delay slots in the queue, we need to delete the oldest ones.
		while (ReadyQueue.size() > Delay)
			DeleteSlotAndPopFromQueue(ReadyQueue);
		if (ReadyQueue.empty())
			return nullptr;
		auto slotPtr = &*ReadyQueue.front();
		ReadyQueue.pop();
		return slotPtr;
	}
	void EndPop(SlotBase& popped)
	{
		// FreeQueue should be normally empty, but if Slot count is not enough in total, then we can keep FreeQueue non-empty
		while (Slots.size() > GetRequiredSlotCount() && !FreeQueue.empty())
			DeleteSlotAndPopFromQueue(FreeQueue);

		if (AccountForActiveSlot)
		{
			if (ActiveSlot)
				FreeQueue.push(*ActiveSlot);
			ActiveSlot = &popped;
		}
		else
			FreeQueue.push(popped);
	}

	void AddResource(std::unique_ptr<SlotBase> slot)
	{
		if (Slots.size() >= Delay + 1)
			return;
		FreeQueue.push(*slot);
		Slots.push_back(std::move(slot));
	}

	size_t GetRequiredSlotCount() 
	{ 
		return Delay + (AccountForActiveSlot ? 1 : 0); 
	}

	bool AreSlotsCompatibleWith(nosBuffer const& buf) const
	{
		if (Slots.empty())
			return true;
		for (const auto& slot : Slots)
			if (!slot->IsSlotCompatible(buf))
				return false;
		return true;
	}
};



struct TriviallyCopyableSlot : SlotBase
{
	using SlotBase::SlotBase;
	void CopyFrom(nosBuffer const& other, uuid const& nodeId) override
	{
		Buffer = other;
	}
	bool IsSlotCompatible(nosBuffer const& buf) const override
	{ 
		return true;
	}
};

template <typename T>
requires std::is_same_v<T, nosTextureInfo> || std::is_same_v<T, nosBufferInfo>
struct ResourceSlot : SlotBase
{
	vkss::Resource Res;
	nosTextureFieldType FieldType{};
	ResourceSlot(nosBuffer const& buf)
		: SlotBase(buf),
		  Res(*vkss::Resource::Create(
			  [](nosBuffer const& buf) -> T {
				  if constexpr (std::is_same_v<T, nosBufferInfo>)
				  {
					  auto desc =
						  vkss::ConvertToResourceInfo(*InterpretPinValue<sys::vulkan::Buffer>(buf.Data)).Info.Buffer;
					  desc.Usage = nosBufferUsage(desc.Usage | nosBufferUsage(NOS_BUFFER_USAGE_TRANSFER_SRC | NOS_BUFFER_USAGE_TRANSFER_DST));
					  return desc;
				  }
				  if constexpr (std::is_same_v<T, nosTextureInfo>)
				  {
					  return vkss::DeserializeTextureInfo(buf.Data).Info.Texture;
				  }
			  }(buf),
			  "Delay Resource"))
	{
	}

	void CopyFrom(nosBuffer const& other, uuid const& nodeId) override
	{
		nosCmd cmd{};
		nosCmdBeginParams beginParams{
			.Name = NOS_NAME_STATIC("Delay Copy"),
			.AssociatedNodeId = nodeId,
			.OutCmdHandle = &cmd
		};
		nosResourceShareInfo src{};
		if constexpr (std::is_same_v<T, nosBufferInfo>)
		{
			src = vkss::ConvertToResourceInfo(*InterpretPinValue<sys::vulkan::Buffer>(other.Data));
			FieldType = src.Info.Buffer.FieldType;
		}
		if constexpr (std::is_same_v<T, nosTextureInfo>)
		{
			src = vkss::DeserializeTextureInfo(other.Data);
			FieldType = src.Info.Texture.FieldType;
		}
		nosVulkan->Begin(&beginParams);
		nosVulkan->Copy(cmd, &src, &Res, nullptr);
		nosVulkan->End(cmd, nullptr);
		Buffer = Res.ToPinData();
	}

	bool IsSlotCompatible(nosBuffer const& buf) const override
	{
		if constexpr (std::is_same_v<T, nosBufferInfo>)
		{
			auto vkBuf = vkss::ConvertToResourceInfo(*InterpretPinValue<sys::vulkan::Buffer>(buf.Data));
			if (vkBuf.Info.Buffer.Size != Res.Info.Buffer.Size ||
				vkBuf.Info.Buffer.Alignment != Res.Info.Buffer.Alignment ||
				vkBuf.Info.Buffer.Usage != Res.Info.Buffer.Usage ||
				vkBuf.Info.Buffer.MemoryFlags != Res.Info.Buffer.MemoryFlags)
				return false;
		}
		if constexpr (std::is_same_v<T, nosTextureInfo>)
		{
			auto tex = vkss::DeserializeTextureInfo(buf.Data);
			if (tex.Info.Texture.Format != Res.Info.Texture.Format ||
				tex.Info.Texture.Width != Res.Info.Texture.Width ||
				tex.Info.Texture.Height != Res.Info.Texture.Height ||
				tex.Info.Texture.Usage != Res.Info.Texture.Usage || tex.Info.Texture.Filter != Res.Info.Texture.Filter)
				return false;
		}
		return true;
	}

	uint64_t GetPhaseCount() const override
	{
		// For 1.3: An auxiliary field type used to avoid continuous path restarts caused by field type mismatches in RingBuffer.
		//          The delay node will report a warning but won't set correct output field type because of this.
		return vkss::IsTextureFieldTypeInterlaced(FieldType) ? 2 : 1;
	}

	std::optional<std::string> HasPhaseCountDelayMismatchWarning(uint64_t delay) const override
	{
		if (GetPhaseCount() <= 1 || (delay % GetPhaseCount() == 0))
			return std::nullopt;
		return "WARNING:\n\tOdd delay values cause field mismatch\n\ton interlaced signals. Use even numbers only.";
	}
};

struct DelayNode : NodeContext
{
    nosName TypeName = NSN_TypeNameGeneric;
	DelayQueue Queue;

	enum class Status
	{
		Ok,
		WarnDelayIsNotMultipleOfPhaseCount
	} CurrentStatus = Status::Ok;

	DelayNode(nosFbNodePtr node) : NodeContext(node), Queue(0)
	{
		for (auto pin : *node->pins())
		{
			auto name = nos::Name(pin->name()->c_str());
			if (NSN_Output == name)
			{
				if (pin->type_name()->c_str() == NSN_TypeNameGeneric.AsString())
					continue;
				SetType(nos::Name(pin->type_name()->c_str()));
			}
		}
	}


	void OnPinValueChanged(nos::Name pinName, uuid const& pinId, nosBuffer value) override
	{
		if (NSN_Delay == pinName)
		{
			Queue.Delay = *static_cast<uint32_t*>(value.Data);
		}
		if(NSN_TypeNameGeneric == TypeName)
			return;
	}

	void OnPinUpdated(nosPinUpdate const* update) override
	{
		if (TypeName != NSN_TypeNameGeneric)
			return;
		if (update->UpdatedField == NOS_PIN_FIELD_TYPE_NAME)
		{
			if (update->PinName != NSN_Input)
				return;
			SetType(update->TypeName);
		}
	}

	void SetStatus(Status newStatus, std::optional<std::string> warningMsg = std::nullopt)
	{
		if (CurrentStatus == newStatus)
			return;
		CurrentStatus = newStatus;
		switch (CurrentStatus)
		{
		case Status::Ok: {
			ClearNodeStatusMessages();
			return;
		}
		case Status::WarnDelayIsNotMultipleOfPhaseCount: {
			SetNodeStatusMessage(warningMsg.value_or("WARNING:\n\tDelay value is not\n\ta multiple of input signal phase "
													 "count.\n\tThis may cause phase mismatch problems downstream."),
								 fb::NodeStatusMessageType::WARNING);
			return;
		}
		default: return;
		}
	}

	void SetType(nos::Name typeName)
	{
		TypeName = typeName;
		Queue.AccountForActiveSlot = TypeName == NSN_TextureTypeName || TypeName == NSN_BufferTypeName;
	}

	nosResult ExecuteNode(nosNodeExecuteParams* params) override
	{
		if (NSN_TypeNameGeneric == TypeName)
			return NOS_RESULT_FAILED;
		nos::NodeExecuteParams execParams(params);

		auto& inputBuffer = *execParams[NSN_Input].Data;
		auto delay = *InterpretPinValue<uint32_t>(*execParams[NSN_Delay].Data);

		if (0 == delay)
		{
			Queue.Clear();
			SetStatus(Status::Ok);
			SetPinValue(NSN_Output, inputBuffer);
			return NOS_RESULT_SUCCESS;
		}

		if (auto popSlot = Queue.BeginPop())
		{
			SetPinValue(NSN_Output, popSlot->Buffer);
			Queue.EndPop(*popSlot);
		}
		Queue.ClearIfIncompatibleData(inputBuffer);
		if (!Queue.HasFree())
		{
			std::unique_ptr<SlotBase> slot;
			if (TypeName == NSN_TextureTypeName)
				slot = std::make_unique<ResourceSlot<nosTextureInfo>>(inputBuffer);
			else if (TypeName == NSN_BufferTypeName)
				slot = std::make_unique<ResourceSlot<nosBufferInfo>>(inputBuffer);
			else
				slot = std::make_unique<TriviallyCopyableSlot>(inputBuffer);
			Queue.AddResource(std::move(slot));
		}
		if (auto pushSlot = Queue.BeginPush())
		{
			pushSlot->CopyFrom(inputBuffer, NodeId);
			if (auto warningMsg = pushSlot->HasPhaseCountDelayMismatchWarning(delay))
				SetStatus(Status::WarnDelayIsNotMultipleOfPhaseCount, warningMsg);
			else
				SetStatus(Status::Ok);
			Queue.EndPush(*pushSlot);
		}
		return NOS_RESULT_SUCCESS;
	}
};

nosResult RegisterDelay(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NSN_Delay, DelayNode, fn)
	return NOS_RESULT_SUCCESS;
}

} // namespace nos