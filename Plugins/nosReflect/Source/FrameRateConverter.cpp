#include "RingBuffer.hpp"

namespace nos::reflect
{

struct FrameRateConverterNode : NodeContext
{
	nos::Name TypeName = NSN_TypeNameGeneric;

	RingBuffer<std::unique_ptr<ObjectSlot>> Ring;
	uint32_t Capacity = 1;
	uint32_t RemainingRepeatCount = 0;
	fb::vec2u Ratio = {1, 1};

	bool CapacityUpdatedViaPathCommand = false;
	
	FrameRateConverterNode() : Ring(1, RingBufferServeMode::WaitUntilFull)
	{
		Ring.Reset(Capacity);
		AddPinValueWatcher<uint32_t>(NOS_NAME("Capacity"), [this](const uint32_t* newCapacity, std::optional<const uint32_t*> oldCapacity)
		{
			if (*newCapacity != Capacity)
			{
				Capacity = std::max(1u, *newCapacity);
				if (*newCapacity != Capacity)
				{
					nosEngine.LogW("%s: Capacity cannot be %lu.",  GetItemPath(NodeId).value_or("<unknown>").c_str(), *newCapacity);
					SetPinValue(NOS_NAME("Capacity"), Capacity);
					return;
				}
				if (!CapacityUpdatedViaPathCommand)
				{
					nosPathCommand ringSizeChange{.Event = NOS_RING_SIZE_CHANGE, .RingSize = Capacity};
					nosEngine.SendPathCommand(*GetPinId(NSN_Input), ringSizeChange);
					CapacityUpdatedViaPathCommand = false;
				}
				SendPathRestart(NSN_Input);
			}
		});
		AddPinValueWatcher<fb::vec2u>(NOS_NAME("Ratio"), [this](const fb::vec2u* newRatio, std::optional<const fb::vec2u*> oldRatio)
		{
			if (*newRatio != Ratio)
			{
				if (newRatio->x() == 0 || newRatio->y() == 0)
				{
					nosEngine.LogW("%s: Ratio components cannot be 0.",  GetItemPath(NodeId).value_or("<unknown>").c_str());
					SetPinValue(NOS_NAME("Ratio"), Ratio);
					return;
				}
				Ratio = *newRatio;
				SendPathRestart(NodeId);
			}
		});
	}

	void SendRingStats(const char* state) const
	{
		auto nodeName = NodeName.AsString();
		nosEngine.WatchLog((nodeName + " Size").c_str(), std::to_string(Ring.GetSize()).c_str());
		nosEngine.WatchLog((nodeName + " Capacity").c_str(), std::to_string(Ring.GetCapacity()).c_str());
		nosEngine.WatchLog((nodeName + " State").c_str(), state);
	}

	void OnPathStart() override
	{
		Ring.Reset(Capacity);
		RemainingRepeatCount = Capacity - 1;
		SendScheduleRequest(Capacity);
	}

	void OnPathStop() override
	{
		Ring.Shutdown();
	}

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

	nosResult CopyFrom(nosCopyFromInfo* cpy) override
	{
		SendRingStats("Pre Begin Pop");
		if (Ring.GetMode() == RingBufferServeMode::WaitUntilFull)
		{
			if (RemainingRepeatCount > 0)
			{
				--RemainingRepeatCount;
				return NOS_RESULT_SUCCESS;
			}
		}
	uint32_t popCount = Ratio.y();
	std::vector<ObjectRef> outputObjectRefs;
	uint64_t frameNumber = 0;
	while (popCount > 0)
	{
		std::unique_ptr<ObjectSlot>* srcSlot;
		{
			ScopedProfilerEvent _({ .Name = "Wait For Read" });
			srcSlot = Ring.BeginPop(100);
		}
		if (srcSlot && *srcSlot)
		{
			auto& slot = *srcSlot;
			frameNumber = slot->FrameNumber;
			outputObjectRefs.push_back(std::move(slot->Object));
			Ring.EndPop();
			SendRingStats("Post Begin Pop");
		}
		else if (Ring.IsShuttingDown())
		{
			return NOS_RESULT_FAILED;
		}
		else
		{
			// Timeout
			if (outputObjectRefs.empty())
				return NOS_RESULT_PENDING;
			break;
		}
		--popCount;
	}
	if (!outputObjectRefs.empty())
	{
		// Convert ObjectRefs to IDs for the API call
		std::vector<nosObjectId> outputObjects;
		outputObjects.reserve(outputObjectRefs.size());
		for (const auto& ref : outputObjectRefs)
			outputObjects.push_back(ref.GetObjectId());
		
		ObjectRef outputArrayObject;
		auto res = nosEngine.ObjectAPI->CreateArrayObject(
			TypeName, outputObjects.data(), outputObjects.size(), &outputArrayObject.GetStorage());
		if (res != NOS_RESULT_SUCCESS)
			return res;
		SetPinObject(NSN_Output, outputArrayObject);
		SendScheduleRequest(1);
		return NOS_RESULT_SUCCESS;
	}
		return NOS_RESULT_PENDING;
	}

	void OnPathCommand(const nosPathCommand* command) override
	{
		switch (command->Event)
		{
		case NOS_RING_SIZE_CHANGE: {
				if (command->RingSize == 0)
				{
					nosEngine.LogW((GetDisplayName() + " capacity cannot be 0.").c_str());
					return;
				}
				CapacityUpdatedViaPathCommand = true;
				SetPinValue(NOS_NAME("Capacity"), command->RingSize);
				break;
		}
		default:
			return;
		}
	}

	nosResult OnResolvePinDataTypes(nosResolvePinDataTypesParams* params) override
	{
		TypeInfo typeInfo(params->IncomingTypeName);
		if (typeInfo->BaseType != NOS_BASE_TYPE_ARRAY)
		{
			strncpy(params->OutErrorMessage, "Connected pin type must be an array type!", 42);
			return NOS_RESULT_FAILED;
		}
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
		SendPathRestart(NodeId);
	}

	nosResult ExecuteNode(NodeExecuteParams const& params) override
	{
		if (NSN_TypeNameGeneric == TypeName)
			return NOS_RESULT_FAILED;
		ArrayObjectRef inputArrayObject = params.GetPinObject(NSN_Input);
		if (!inputArrayObject)
			return NOS_RESULT_FAILURE;
		auto capacity = *InterpretObject<uint32_t>(*params[NOS_NAME("Capacity")].Object);
		capacity = std::max(1u, capacity);

		SendRingStats("Pre Push");
		uint32_t pushCount = Ratio.x();
		for (auto inputObject : inputArrayObject)
		{
			std::unique_ptr<ObjectSlot>* dstSlot;
			{
				ScopedProfilerEvent _({ .Name = "Wait For Empty Slot" });
				dstSlot = Ring.BeginPush(100);
			}
			if (dstSlot)
			{
				if (!*dstSlot)
					*dstSlot = std::make_unique<ObjectSlot>(inputArrayObject);
				auto& slot = *dstSlot;
				if (!slot->IsDestinationCompatibleWith(inputArrayObject))
				{
					SendPathRestart(NSN_Input);
					return NOS_RESULT_FAILURE;
				}
				slot->FrameNumber = params.FrameNumber;
				auto res = slot->CopyFrom(inputObject);
				Ring.EndPush();
				SendRingStats("Post Push");
				if (res != NOS_RESULT_SUCCESS)
					return res;
			}
			else if (Ring.IsShuttingDown())
			{
				return NOS_RESULT_FAILED;
			}
			else
			{
				// Timeout
				return NOS_RESULT_PENDING;
			}
			if (pushCount == 0)
				nosEngine.LogW("%s: Item count in array exceeded input ratio!",  GetItemPath(NodeId).value_or("<unknown>").c_str());
			if (pushCount != 0)
				--pushCount;
		}
		if (pushCount > 0)
			nosEngine.LogW("%s: Fewer items in input array than specified in ratio!",  GetItemPath(NodeId).value_or("<unknown>").c_str());
		return NOS_RESULT_SUCCESS;
	}
};

nosResult RegisterFrameRateConverter(nosNodeFunctions* node)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("FrameRateConverter"), FrameRateConverterNode, node)
	return NOS_RESULT_SUCCESS;
}

}