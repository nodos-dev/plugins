// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include "TypeCommon.h"

#include <nosVulkanSubsystem/nosVulkanSubsystem.h>
#include <nosVulkanSubsystem/Helpers.hpp>

namespace nos::reflect
{

struct SlotBase
{
	ObjectRef Object;
	virtual ~SlotBase() = default;
	SlotBase(const SlotBase&) = delete;
	SlotBase& operator=(const SlotBase&) = delete;
	virtual void CopyFrom(nosObjectHandle obj, uuid const& nodeId) = 0;
	virtual bool IsSlotCompatible(nosObjectHandle obj) const = 0;
protected:
	SlotBase(ObjectRef object) : Object(std::move(object)) {}
};

struct DelayQueue {
	std::vector<std::unique_ptr<SlotBase>> Slots;
	std::queue<Ref<SlotBase>> ReadyQueue;
	// This is the actively used slot, which is the output pin's value, if AccountForActiveSlot is true.
	SlotBase* ActiveSlot{};
	std::queue<Ref<SlotBase>> FreeQueue;
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

		if (ActiveSlot)
			FreeQueue.push(*ActiveSlot);
		ActiveSlot = &popped;
	}

	void AddResource(std::unique_ptr<SlotBase> slot)
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

struct PODSlot : SlotBase
{
	PODSlot(nosObjectHandle obj) : SlotBase(obj) {}
	void CopyFrom(nosObjectHandle obj, uuid const& nodeI) override { Object = obj; }
	bool IsSlotCompatible(nosObjectHandle obj) const override { return true; }
};

ObjectRef CreateVkDelayDestinationResource(nosObjectHandle fromObj)
{
	auto inRes = vkss::GetResourceInfo(fromObj);
	if (!inRes)
		return {};
	if (inRes->Type == nosResourceType::NOS_RESOURCE_TYPE_BUFFER)
	{
		inRes->Buffer.Usage =
			nosBufferUsage(inRes->Buffer.Usage | NOS_BUFFER_USAGE_TRANSFER_SRC | NOS_BUFFER_USAGE_TRANSFER_DST);
	}
	else if (inRes->Type == nosResourceType::NOS_RESOURCE_TYPE_TEXTURE)
	{
		inRes->Texture.Usage =
			nosImageUsage(inRes->Texture.Usage | NOS_IMAGE_USAGE_TRANSFER_SRC | NOS_IMAGE_USAGE_TRANSFER_DST);
	}
	return vkss::CreateResource(*inRes, "Delay Resource");
}

struct ResourceSlot : SlotBase
{
	nosResourceInfo ObjResInfo{};
	ResourceSlot(nosObjectHandle fromObj) : SlotBase(CreateVkDelayDestinationResource(fromObj))
	{
		nosVulkan->GetResourceInfo(fromObj, &ObjResInfo, nullptr);
	}

	void CopyFrom(nosObjectHandle fromObj, uuid const& nodeId) override
	{
		// TODO: Interlaced.
		nosCmd cmd{};
		nosCmdBeginParams beginParams{
			.Name = NOS_NAME_STATIC("Delay Copy"), .AssociatedNodeId = nodeId, .OutCmdHandle = &cmd};
		nosVulkan->Begin(&beginParams);
		nosVulkan->Copy(cmd, fromObj, Object, nullptr);
		nosVulkan->End(cmd, nullptr);
	}

	bool IsSlotCompatible(nosObjectHandle fromObj) const override
	{
		nosResourceInfo fromRes{};
		if (nosVulkan->GetResourceInfo(fromObj, &fromRes, nullptr) != NOS_RESULT_SUCCESS)
			return false;
		if (fromRes.Type == NOS_RESOURCE_TYPE_BUFFER)
		{
			if (fromRes.Buffer.Size != ObjResInfo.Buffer.Size ||
				fromRes.Buffer.Alignment != ObjResInfo.Buffer.Alignment ||
				fromRes.Buffer.Usage != ObjResInfo.Buffer.Usage ||
				fromRes.Buffer.MemoryFlags != ObjResInfo.Buffer.MemoryFlags)
				return false;
		}
		else
		{
			if (fromRes.Texture.Format != ObjResInfo.Texture.Format ||
				fromRes.Texture.Width != ObjResInfo.Texture.Width ||
				fromRes.Texture.Height != ObjResInfo.Texture.Height ||
				fromRes.Texture.Usage != ObjResInfo.Texture.Usage)
				return false;
		}
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
		if(NSN_TypeNameGeneric == TypeName)
			return;
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
	}

	nosResult ExecuteNode(nosNodeExecuteParams* params) override
	{
		if (NSN_TypeNameGeneric == TypeName)
			return NOS_RESULT_FAILED;
		nos::NodeExecuteParams execParams(params);

		auto& inputObject = *execParams[NSN_Input].ObjectHandle;
		auto delay = *InterpretObject<uint32_t>(*execParams[NSN_Delay].ObjectHandle);

		if (0 == delay)
		{
			Queue.Clear();
			SetPinObject(NSN_Output, inputObject);
			return NOS_RESULT_SUCCESS;
		}

		if (auto popSlot = Queue.BeginPop())
		{
			SetPinObject(NSN_Output, popSlot->Object);
			Queue.EndPop(*popSlot);
		}
		Queue.ClearIfIncompatibleData(inputObject);
		if (!Queue.HasFree())
		{
			std::unique_ptr<SlotBase> slot;
			if (TypeName == NSN_TextureTypeName || TypeName == NSN_BufferTypeName)
				slot = std::make_unique<ResourceSlot>(inputObject);
			else
				slot = std::make_unique<PODSlot>(inputObject);
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