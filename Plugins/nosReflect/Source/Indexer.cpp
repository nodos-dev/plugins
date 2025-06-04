// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include "TypeCommon.h"

namespace nos::reflect
{
struct Indexer : NodeContext
{
	std::optional<nos::TypeInfo> Type = std::nullopt;

    uint32_t Index = 0;
	uint32_t ArraySize = 0;
    
    nosResult OnCreate(nosFbNodePtr inNode) override
    {
        for (auto pin : *inNode->pins())
        {
			if(pin->name()->string_view() == NSN_Output)
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
		UpdateInputVectorSize();
	}

	bool SetIndex(uint32_t newIndex)
    {
		Index = newIndex;
    	if (Index >= ArraySize)
    	{
			SetNodeStatusMessages({{{}, "Array index out of bounds", fb::NodeStatusMessageType::FAILURE, "", 5, true}});
			SetPinOrphanState(NSN_Output, fb::PinOrphanStateType::PASSIVE, "Array index out of bounds");
    		return false;
    	}
		ClearOutputState();
    	return true;
    }

	void ClearOutputState()
	{
		SetPinOrphanState(NSN_Output, fb::PinOrphanStateType::ACTIVE, "");
		ClearNodeStatusMessages();
	}

	bool UpdateInputVectorSize() {
		std::vector<uint8_t> data;

		if (auto buf = GetDefaultValueOfType(Type->TypeName))
		{
			data = std::vector<uint8_t>{(uint8_t*)buf->Data(), (uint8_t*)buf->Data() + buf->Size()};
		}

		std::vector<const void*> datas = { data.data() };

		auto inPin = GetPin(NSN_Input);
		if (!inPin || !Type)
			return false;

		//nosEngine.SetPinValue(inPin->Id, GenerateVector(*Type, datas));
		return true;
	}
	
    nosResult ExecuteNode(nosNodeExecuteParams* params) override
    {
		if (!Type)
			return NOS_RESULT_FAILED;

		auto pins = NodeExecuteParams(params);
		if (!pins[NSN_Input].Data)
		{
			UpdateInputVectorSize();
		}

		auto vec = InterpretPinValue<VectorPinData<uint8_t>>(*pins[NSN_Input].Data);
    	ArraySize = vec->size();
		if (!SetIndex(*(uint32_t*)pins[NSN_Index].Data->Data))
			return NOS_RESULT_FAILED;
		auto ID = pins[NSN_Output].Id;
		auto& type = *Type;

		nosQueryBufferParams queryParams;
		{
			queryParams.TypeName = nos::Name("[" + nos::Name(type->TypeName).AsString() + "]");
			queryParams.Buffer = *pins[NSN_Input].Data;
			nosDataPathComponent queryPath;
			queryPath.ComponentType = nosDataPathComponentType::NOS_DATA_PATH_ARRAY_ELEMENT;
			queryPath.Component.ArrayIndex = Index;
			queryParams.Path = &queryPath;
			queryParams.PathLength = 1;
		}
		auto element = QueryBuffer(queryParams);
		if (!element)
		{
			SetNodeStatusMessages({ {{}, "Failed to query buffer", fb::NodeStatusMessageType::FAILURE, "", 5, true} });
			return NOS_RESULT_FAILED;
		}
		nosEngine.SetPinValue(ID, *element);
		return NOS_RESULT_SUCCESS;
    }

	void OnPinValueChanged(nos::Name pinName, uuid const& pinId, nosBuffer value) override
    {
        if (pinName == NSN_Index)
        {
        	SetIndex(*(uint32_t*)value.Data);
		}
        else if (pinName == NSN_Input)
        {
			if (!Type)
				return;
            ArraySize = InterpretPinValue<VectorPinData<uint8_t>>(value)->size();
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