// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include "TypeCommon.h"

#include <nosVulkanSubsystem/nosVulkanSubsystem.h>
#include <nosVulkanSubsystem/Helpers.hpp>
#include <nosTransfer/nosTransfer.h>

namespace nos::reflect
{

struct Slot
{
	nosTransferCopyDestination Handle;
	Slot(ObjectRef src)
	{
		nosTransfer->CreateCopyDestination(src, &Handle);
	}
	~Slot()
	{
		nosTransfer->ReleaseCopyDestination(Handle);
	}
	Slot(const Slot&) = delete;
	Slot& operator=(const Slot&) = delete;
	nosResult CopyFrom(nosObjectHandle obj, uuid const& nodeId)
	{
		return nosTransfer->Copy(obj, Handle);
	}
	bool IsSlotCompatible(nosObjectHandle obj) const
	{
		return nosTransfer->CanCopy(obj, Handle) == NOS_TRUE;
	}
	ObjectRef GetObject() const
	{
		ObjectRef obj{};
		if (nosTransfer->GetObjectHandle(Handle, &obj.Handle) != NOS_RESULT_SUCCESS)
			return ObjectRef();
		return obj;
	}
};

struct DelayQueue {
	std::vector<std::unique_ptr<Slot>> Slots;
	std::queue<Ref<Slot>> ReadyQueue;
	// This is the actively used slot, which is the output pin's value, if AccountForActiveSlot is true.
	Slot* ActiveSlot{};
	std::queue<Ref<Slot>> FreeQueue;
	uint32_t Delay = 0;
	DelayQueue() {}
	DelayQueue(const DelayQueue&) = delete;
	void Clear()
	{
		ActiveSlot = nullptr;
		ReadyQueue = {};
		FreeQueue = {};
		Slots.clear(); 
	}

	void ClearIfIncompatibleData(nosObjectHandle obj)
	{
		if (AreSlotsCompatibleWith(obj))
			return;
		Clear();
	}

	bool HasFree() const 
	{
		return !FreeQueue.empty(); 
	}

	void DeleteSlotAndPopFromQueue(std::queue<Ref<Slot>>& queue)
	{
		if (!queue.empty())
		{
			auto slotPtr = queue.front();
			queue.pop();
			std::erase_if(Slots, [slotPtr](const std::unique_ptr<Slot>& slot) { return slot.get() == &slotPtr; });
		}
	}

	Slot* BeginPush()
	{
		if (!HasFree())
			return nullptr;
		auto slotPtr = &*FreeQueue.front();
		FreeQueue.pop();
		return slotPtr;
	}
	void EndPush(Slot& slot)
	{
		ReadyQueue.push(slot);
		// If there are more than Delay slots in the queue, we need to delete the oldest ones.
		while (ReadyQueue.size() > Delay)
			DeleteSlotAndPopFromQueue(ReadyQueue);
	}
	Slot* BeginPop()
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
	void EndPop(Slot& popped)
	{
		// FreeQueue should be normally empty, but if Slot count is not enough in total, then we can keep FreeQueue non-empty
		while (Slots.size() > GetRequiredSlotCount() && !FreeQueue.empty())
			DeleteSlotAndPopFromQueue(FreeQueue);

		if (ActiveSlot)
			FreeQueue.push(*ActiveSlot);
		ActiveSlot = &popped;
	}

	void AddResource(std::unique_ptr<Slot> slot)
	{
		if (Slots.size() >= GetRequiredSlotCount())
			return;
		FreeQueue.push(*slot);
		Slots.push_back(std::move(slot));
	}

	size_t GetRequiredSlotCount() 
	{ 
		return Delay + 1; 
	}

	bool AreSlotsCompatibleWith(nosObjectHandle obj) const
	{
		if (Slots.empty())
			return true;
		for (const auto& slot : Slots)
			if (!slot->IsSlotCompatible(obj))
				return false;
		return true;
	}
};

struct DelayNode : NodeContext
{
    nosName TypeName = NSN_TypeNameGeneric;
	DelayQueue Queue{};

	nosResult OnCreate(nosFbNodePtr node) override
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
		return NOS_RESULT_SUCCESS;
	}

	void OnPinObjectHandleChanged(nos::Name pinName, uuid const& pinId, nosObjectHandle handle) override
	{
		if (NSN_Delay == pinName)
		{
			Queue.Delay = *InterpretObject<uint32_t>(handle);
		}
	}

	nosResult OnResolvePinDataTypes(nosResolvePinDataTypesParams* params) override
	{
		if (TypeName != NSN_TypeNameGeneric)
			return NOS_RESULT_FAILED;
		nos::TypeInfo incomingType(params->IncomingTypeName);
		for (int i = 0; i < incomingType->AttributeCount; ++i)
			if (incomingType->Attributes[i].Name == NOS_NAME("skip_delay"))
				return NOS_RESULT_FAILED;
		return NOS_RESULT_SUCCESS;
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

	void SetType(nos::Name typeName)
	{
		TypeName = typeName;
	}

	nosResult ExecuteNode(NodeExecuteParams const& params) override
	{
		if (NSN_TypeNameGeneric == TypeName)
			return NOS_RESULT_FAILED;

		ObjectRef inputObject = *params[NSN_Input].ObjectHandle;
		auto delay = *InterpretObject<uint32_t>(*params[NSN_Delay].ObjectHandle);

		if (0 == delay)
		{
			Queue.Clear();
			SetPinObject(NSN_Output, inputObject);
			return NOS_RESULT_SUCCESS;
		}

		if (auto popSlot = Queue.BeginPop())
		{
			SetPinObject(NSN_Output, popSlot->GetObject());
			Queue.EndPop(*popSlot);
		}
		Queue.ClearIfIncompatibleData(inputObject);
		if (!Queue.HasFree())
		{
			auto slot = std::make_unique<Slot>(inputObject);
			Queue.AddResource(std::move(slot));
		}
		if (auto pushSlot = Queue.BeginPush())
		{
			pushSlot->CopyFrom(inputObject, NodeId);
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