// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include "TypeCommon.h"

namespace nos::reflect
{
struct Indexer : NodeContext
{
	std::optional<nos::TypeInfo> Type = std::nullopt;

	uint32_t Index = 0;
	uint32_t ArraySize = 0;
	enum class IndexState
	{
		None = 0,
		Valid = 1,
		Invalid = 2,
	};
	IndexState LastState = IndexState::None;

	nosResult OnCreate(nosFbNodePtr inNode) override
	{
		for (auto pin : *inNode->pins())
		{
			if (pin->name()->string_view() == NSN_Output)
			{
				if (pin->type_name()->string_view() != NSN_TypeNameGeneric)
				{
					Type = nos::TypeInfo(nos::Name(pin->type_name()->string_view()));
				}
			}
			else if (pin->name()->string_view() == NSN_Index)
			{
				if (flatbuffers::IsFieldPresent(pin, fb::Pin::VT_DATA))
					Index = *(uint32_t*)pin->data()->Data();
			}
		}
		return NOS_RESULT_SUCCESS;
	}

	nosResult OnResolvePinDataTypes(nosResolvePinDataTypesParams* params) override
	{
		if (params->InstigatorPinName == NSN_Input)
		{
			nos::TypeInfo info(params->IncomingTypeName);
			if (info->BaseType != NOS_BASE_TYPE_ARRAY)
			{
				strcpy(params->OutErrorMessage, "Input pin must be an array type");
				return NOS_RESULT_FAILED;
			}

			nos::Name elementName = info->ElementType->TypeName;

			for (size_t i = 0; i < params->PinCount; i++)
			{
				auto& pinInfo = params->Pins[i];
				if (pinInfo.Name == NSN_Output)
				{
					pinInfo.OutResolvedTypeName = elementName;
					break;
				}
			}

			return NOS_RESULT_SUCCESS;
		}
		else if (params->InstigatorPinName == NSN_Output)
		{
			nos::TypeInfo info(params->IncomingTypeName);
			if (info->BaseType == NOS_BASE_TYPE_ARRAY)
			{
				strcpy(params->OutErrorMessage, "Output pin must not be an array type");
				return NOS_RESULT_FAILED;
			}
			nos::Name arrayName = nos::Name("[" + nos::Name(info.TypeName).AsString() + "]");
			for (size_t i = 0; i < params->PinCount; i++)
			{
				auto& pinInfo = params->Pins[i];
				if (pinInfo.Name == NSN_Input)
				{
					pinInfo.OutResolvedTypeName = arrayName;
					break;
				}
			}

			return NOS_RESULT_SUCCESS;
		}
		return NOS_RESULT_FAILED;
	}

	void OnPinUpdated(nosPinUpdate const* update) override
	{
		if (Type || update->UpdatedField != NOS_PIN_FIELD_TYPE_NAME)
			return;
		if (update->PinName == NSN_Output)
		{
			Type = nos::TypeInfo(update->TypeName);
		}
		else if (update->PinName == NSN_Input)
		{
			auto newTypeName = nos::Name(update->TypeName);
			auto typeInfo = nos::TypeInfo(newTypeName);
			if (typeInfo->BaseType == NOS_BASE_TYPE_ARRAY)
			{
				newTypeName = typeInfo->ElementType->TypeName;
			}
			Type = nos::TypeInfo(newTypeName);
		}
	}

	void UpdateIndexState(IndexState newState)
	{
		if (newState == LastState)
			return;
		LastState = newState;
		if (newState == IndexState::Valid)
		{
			SetPinOrphanState(NSN_Output, fb::PinOrphanStateType::ACTIVE);
			ClearNodeStatusMessages();
		}
		else
		{
			SetPinOrphanState(NSN_Output, fb::PinOrphanStateType::PASSIVE, "Array index out of bounds");
			SetNodeStatusMessages({ {{}, "Array index out of bounds", fb::NodeStatusMessageType::FAILURE, "", 5, true} });
		}
	}

	bool SetIndex(uint32_t newIndex)
	{
		Index = newIndex;
		bool isValidIndex = Index < ArraySize;
		UpdateIndexState(isValidIndex ? IndexState::Valid : IndexState::Invalid);
		return isValidIndex;
	}

	nosResult ExecuteNode(NodeExecuteParams const& params) override
	{
		if (!Type)
			return NOS_RESULT_FAILED;
		
		auto vecObj = params.GetPinObject<ArrayObjectRef>(NSN_Input);
		if (!vecObj.IsValid())
		{
			SetNodeStatusMessages({
				{{}, "Input array is not valid", fb::NodeStatusMessageType::FAILURE, "", 5, true}
			});
			return NOS_RESULT_FAILURE;
		}
		
		ArraySize = vecObj.GetSize();

		if (!SetIndex(*params.GetPinData<uint32_t>(NSN_Index)))
			return NOS_RESULT_SUCCESS;

		auto elem = vecObj.GetElement(Index);
		if (!elem || !elem->IsValid())
			return NOS_RESULT_SUCCESS;

		SetPinObject(NSN_Output, *elem);
		return NOS_RESULT_SUCCESS;
	}

	void OnPinObjectChanged(nos::Name pinName, uuid const& pinId, nosObjectId newObj) override
	{
		if (pinName == NSN_Index)
		{
			auto value = GetObjectDataView(newObj);
			if (!value)
				return;
			SetIndex(*static_cast<const uint32_t*>(value->Data));
		}
		else if (pinName == NSN_Input)
		{
			if (!Type)
				return;
			auto vecObj = ArrayObjectRef::FromObjectId(newObj);
			if (!vecObj.IsValid())
				return;
			ArraySize = vecObj.GetSize();
			SetIndex(Index);
		}
	}
};

nosResult RegisterIndexer(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NSN_Indexer, Indexer, fn);
	return NOS_RESULT_SUCCESS;
}
} // namespace nos
