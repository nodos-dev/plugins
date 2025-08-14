// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include "TypeCommon.h"

#include <nosVulkanSubsystem/nosVulkanSubsystem.h>
#include <nosVulkanSubsystem/Helpers.hpp>

namespace nos::reflect
{

struct Slot
{
	struct DelayNode& Context; 
	nosObjectHandle Handle = nullptr;
	~Slot();
	Slot(const Slot&) = delete;
	Slot& operator=(const Slot&) = delete;
	Slot(DelayNode& context, nosBuffer const& buf);
	void CopyFrom(nosBuffer const& buf, uuid const& nodeId);
	bool IsSlotCompatible(nosBuffer const& buf) const;
	EngineBuffer GetBuffer();
};

struct DelayQueue {
	std::vector<std::unique_ptr<Slot>> Slots;
	std::queue<Ref<Slot>> ReadyQueue;
	// This is the actively used slot, which is the output pin's value, if AccountForActiveSlot is true.
	Slot* ActiveSlot{};
	std::queue<Ref<Slot>> FreeQueue;
	uint32_t Delay = 0;
	bool AccountForActiveSlot = false;
	DelayQueue() {}
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

		if (AccountForActiveSlot)
		{
			if (ActiveSlot)
				FreeQueue.push(*ActiveSlot);
			ActiveSlot = &popped;
		}
		else
			FreeQueue.push(popped);
	}

	void AddResource(std::unique_ptr<Slot> slot)
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

struct DelayNode : NodeContext
{
	nos::Name TypeName = NSN_TypeNameGeneric;
	DelayQueue Queue{};

	nosObjectCopyInterface* CopyInterface = nullptr;
	nosObjectTypeFunctions Functions{};

	nosResult OnCreate(nosFbNodePtr node) override
	{
		for (auto pin : *node->pins())
		{
			auto name = nos::Name(pin->name()->c_str());
			if (NSN_Output == name)
			{
				if (strcmp(pin->type_name()->c_str(), NSN_TypeNameGeneric.AsCStr()) == 0)
					continue;
				SetType(nos::Name(pin->type_name()->c_str()));
			}
		}
		return NOS_RESULT_SUCCESS;
	}

	void OnPinValueChanged(nos::Name pinName, uuid const& pinId, nosBuffer value) override
	{
		if (NSN_Delay == pinName)
		{
			Queue.Delay = *static_cast<uint32_t*>(value.Data);
		}
	}

	nosResult OnResolvePinDataTypes(nosResolvePinDataTypesParams* params) override
	{
		if (TypeName != NSN_TypeNameGeneric)
			return NOS_RESULT_FAILED;
		nos::TypeInfo incomingType(params->IncomingTypeName);
		for (int i = 0; i < incomingType->AttributeCount; ++i)
			if (incomingType->Attributes[i].Name == NOS_NAME_STATIC("skip_delay"))
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
		Queue.AccountForActiveSlot = true; // TODO!
		auto res = nosEngine.GetObjectTypeFunctions(TypeName, &Functions);
		if (res != NOS_RESULT_SUCCESS)
		{
			nosEngine.LogE("Failed to get object type functions for type: %s", TypeName.AsCStr());
			return;
		}
		res = nosEngine.RequestInterfaceImplementation(TypeName, NOS_NAME(nosObjectCopyInterface::Name), reinterpret_cast<void**>(&CopyInterface));
		if (res != NOS_RESULT_SUCCESS)
		{
			nosEngine.LogE("Failed to request copy interface implementation for type: %s", TypeName.AsCStr());
			return;
		}
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
			SetPinValue(NSN_Output, inputBuffer);
			return NOS_RESULT_SUCCESS;
		}

		if (auto popSlot = Queue.BeginPop())
		{
			SetPinValue(NSN_Output, popSlot->GetBuffer());
			Queue.EndPop(*popSlot);
		}
		Queue.ClearIfIncompatibleData(inputBuffer);
		if (!Queue.HasFree())
		{
			auto slot = std::make_unique<Slot>(*this, inputBuffer);
			Queue.AddResource(std::move(slot));
		}
		if (auto pushSlot = Queue.BeginPush())
		{
			pushSlot->CopyFrom(inputBuffer, NodeId);
			Queue.EndPush(*pushSlot);
		}
		return NOS_RESULT_SUCCESS;
	}
};

Slot::Slot(DelayNode& context, nosBuffer const& buf)
	: Context(context)
{
	Context.Functions.Create(
		nosTypedBuffer {
			.TypeName = Context.TypeName,
			.Buffer = buf
		},
		&Handle);
}

Slot::~Slot()
{
	Context.Functions.Release(Handle);
}

void Slot::CopyFrom(nosBuffer const& buf, uuid const& nodeId)
{
	nosObjectHandle inHandle = nullptr;
	Context.Functions.GetHandleFromTypedBuffer(
		nosTypedBuffer {
			.TypeName = Context.TypeName,
			.Buffer = buf
		},
		&inHandle);
	Context.CopyInterface->Copy(inHandle, Handle, nullptr);
	Context.Functions.Release(inHandle);
}

bool Slot::IsSlotCompatible(nosBuffer const& buf) const
{
	nosObjectHandle inHandle = nullptr;
	Context.Functions.GetHandleFromTypedBuffer(
		nosTypedBuffer {
			.TypeName = Context.TypeName,
			.Buffer = buf
		},
		&inHandle);
	bool supports = NOS_OBJECT_COPY_COMPATIBILITY_FLAGS_SUPPORTS_COPY & Context.CopyInterface->GetCopyCompatibility(Handle, inHandle);
	Context.Functions.Release(inHandle);
	return supports;
}

EngineBuffer Slot::GetBuffer()
{
	nosTypedBuffer outBuf;
	Context.Functions.GetTypedBufferFromHandle(Handle, &outBuf);
	return EngineBuffer::FromExisting(outBuf.Buffer);
}

nosResult RegisterDelay(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NSN_Delay, DelayNode, fn)
	return NOS_RESULT_SUCCESS;
}

} // namespace nos