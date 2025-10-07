// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include "TypeCommon.h"

namespace nos::reflect
{
struct BreakNode : NodeContext
{
	std::optional<nos::TypeInfo> Type = std::nullopt;
	nosObjectKind ObjectKind = NOS_OBJECT_KIND_PRIMITIVE;
	size_t ArraySize = 0;

	void OnTypeUpdated(nos::Name typeName, bool preserveDisplayNames)
	{
		if (typeName == NSN_TypeNameGeneric)
			Type = std::nullopt;
		else
		{
			Type = nos::TypeInfo(typeName);
			if (NOS_RESULT_SUCCESS != nosEngine.ObjectAPI->GetObjectKindFromTypeName(typeName, &ObjectKind))
			{
				SetNodeOrphanState(fb::NodeOrphanStateType::ORPHAN, "Invalid type");
				return;
			}
			LoadPins(!preserveDisplayNames);
		}
	}

	nosResult OnCreate(nosFbNodePtr node) override
	{
		for (auto* pin : *node->pins())
		{
			if (pin->show_as() != fb::ShowAs::INPUT_PIN)
				continue;
            auto typeName = nos::Name(pin->type_name()->string_view());
			if (NSN_TypeNameGeneric == typeName)
				break;
			auto ty = nos::TypeInfo(typeName);
			if (!ty)
				// Has typename but not a valid type
				return NOS_RESULT_FAILED;
			OnTypeUpdated(typeName, true);
            break;
        }
		return NOS_RESULT_SUCCESS;
	}

	void OnPinObjectHandleChanged(nos::Name pinName, uuid const& pinId, nosObjectHandle newHandle) override
	{ 
		if (!Type || (*Type)->BaseType != NOS_BASE_TYPE_ARRAY || pinName != NSN_Input)
			return;
		size_t size = 0;
		if (NOS_RESULT_SUCCESS != nosEngine.ObjectAPI->GetArraySize(newHandle, &size))
		{
			SetNodeOrphanState(fb::NodeOrphanStateType::ORPHAN, "Invalid array object");
			return;
		}
		if (size != ArraySize)
		{
			ArraySize = size;
			SetOutputCount();
		}
	}

    void OnPinUpdated(const nosPinUpdate* update) override
	{
		if (Type)
			return;
		if (update->UpdatedField == NOS_PIN_FIELD_TYPE_NAME)
		{
			if (update->PinName != NSN_Input)
				return;
			OnTypeUpdated(update->TypeName, false);
		}
	}

    nosResult OnResolvePinDataTypes(nosResolvePinDataTypesParams* params) override
	{
		auto info = nos::TypeInfo(params->IncomingTypeName);
		switch (info->BaseType)
		{
		case NOS_BASE_TYPE_ARRAY:
		case NOS_BASE_TYPE_STRUCT:
		{
			for (int i = 0; i < info->AttributeCount; ++i)
				if (info->Attributes[i].Name == NOS_NAME_STATIC("skip_break"))
					return NOS_RESULT_FAILED;
			return NOS_RESULT_SUCCESS;
		}
		default: return NOS_RESULT_FAILED;
		}
	};

    void LoadPins(bool preserveDisplayNames)
    {
        flatbuffers::FlatBufferBuilder fbb;
		std::vector<flatbuffers::Offset<PartialPinUpdate>> pinsToUpdate;
		std::vector<::flatbuffers::Offset<nos::fb::Pin>> pinsToCreate;
        std::vector<fb::UUID> pinsToDelete;

		std::unordered_set<nos::Name> acceptedPins;
	
		if (auto* in = GetPin(NSN_Input))
		{
			pinsToUpdate.push_back(CreatePartialPinUpdate(fbb, &in->Id, 0, fb::CreatePinOrphanStateDirect(fbb, fb::PinOrphanStateType::ACTIVE)));
			acceptedPins.insert(in->Name);
		}
		auto& type = *Type;
        if (type->BaseType == NOS_BASE_TYPE_STRUCT)
		{
			for (int i = 0; i < type->FieldCount; ++i)
			{
				auto& field = type->Fields[i];
				acceptedPins.insert(field.Name);
				if (auto pin = GetPin(field.Name))
				{
					if (!pin->IsOrphan && pin->TypeName == field.Type->TypeName)
						continue;
					pinsToUpdate.push_back(CreatePartialPinUpdateDirect(fbb,
																		&pin->Id,
																		0,
																		nos::fb::CreatePinOrphanStateDirect(fbb, fb::PinOrphanStateType::ACTIVE),
																		nos::Name(field.Type->TypeName).AsCStr()));
				}
				else
				{
					uuid newPinId = nosEngine.GenerateID();

					std::vector<uint8_t> data =
						std::vector((const uint8_t*)field.DefaultValue.Data,
									(const uint8_t*)field.DefaultValue.Data + field.DefaultValue.Size);

					pinsToCreate.push_back(fb::CreatePinDirect(fbb,
															   &newPinId,
															   nos::Name(field.Name).AsCStr(),
															   nos::Name(field.Type->TypeName).AsCStr(),
															   fb::ShowAs::OUTPUT_PIN,
															   fb::CanShowAs::OUTPUT_PIN_OR_PROPERTY,
															   0,
															   &data,
															   0,
															   0,
															   0,
															   &data));
				}
			}
		}
        else if (type->BaseType == NOS_BASE_TYPE_ARRAY)
        {
        	size_t i = 0;
        	while (auto pin = GetPin(nos::Name("Output " + std::to_string(i))))
        	{
        		acceptedPins.insert(pin->Name);
        		i++;
        		if (pin->IsOrphan)
        		{
        			pinsToUpdate.push_back(CreatePartialPinUpdate(fbb, &pin->Id, 0, fb::CreatePinOrphanStateDirect(fbb, fb::PinOrphanStateType::ACTIVE)));
        		}
        	}
        	ArraySize = i;
        }
		auto typeName = "Break " + nos::Name(Type->TypeName).AsString();
		for (auto& [id, pin] : Pins)
			if (!acceptedPins.contains(pin.Name))
				pinsToDelete.push_back(id);

        HandleEvent(CreateAppEvent(
			fbb, CreatePartialNodeUpdateDirect(fbb, &NodeId, ClearFlags::NONE, &pinsToDelete, &pinsToCreate, 0, 0, 0, 0, 0, &pinsToUpdate, 0, 0, 0, preserveDisplayNames ? 0 : typeName.c_str())));
    }

	void SetOutputCount()
    { 
    	size_t i = 0;
    	std::vector<fb::UUID> pinsToDelete;
    	std::vector<::flatbuffers::Offset<nos::fb::Pin>> pinsToCreate;
    	while (true)
    	{
    		auto inputName = nos::Name("Output " + std::to_string(i));
    		auto input = GetPin(inputName);
    		if (!input)
    			break;
    		if (i >= ArraySize)
    			pinsToDelete.push_back(input->Id);
    		i++;
    	}
    	flatbuffers::FlatBufferBuilder fbb;
    	if (ArraySize > i)
    	{
    		for (size_t a = 0; a < ArraySize - i; a++)
    		{
    			uuid newPinId = nosEngine.GenerateID();
    			std::vector<uint8_t> vec{};
				auto& type = *Type;
    			if (auto buf = GetDefaultValueOfType(type->ElementType->TypeName))
    			{
					vec = std::vector((const uint8_t*)buf->Data(), (const uint8_t*)buf->Data() + buf->Size());
    			}

    			std::vector<uint8_t> data = std::vector<uint8_t>(type->ByteSize);
    			pinsToCreate.push_back(fb::CreatePinDirect(fbb,
															&newPinId,
															nos::Name("Output " + std::to_string(i + a)).AsCStr(),
															nos::Name(type->ElementType->TypeName).AsCStr(),
															fb::ShowAs::OUTPUT_PIN,
															fb::CanShowAs::OUTPUT_PIN_OR_PROPERTY,
															0,
															&data,
															0,
															0,
															0,
															&data));
    		}
    	}
    	if (!pinsToDelete.empty() || !pinsToCreate.empty())
    		HandleEvent(CreateAppEvent(
				fbb, CreatePartialNodeUpdateDirect(fbb, &NodeId, ClearFlags::NONE, &pinsToDelete, &pinsToCreate)));
    }

	std::unordered_map<uuid, nos::Buffer> LastServedPinValues;

	void SetPinValueCached(const uuid& pinId, nosBuffer value)
	{
		auto it = LastServedPinValues.find(pinId);
		if (it != LastServedPinValues.end() && it->second == value)
			return;
        SetPinValue(pinId, value);
		LastServedPinValues[pinId] = value;
	}
	
    void SetOutputValues(nosBuffer buf)
    {
        auto& type = *Type;
        switch (type->BaseType)
        {
        case NOS_BASE_TYPE_ARRAY: {
        	const flatbuffers::Vector<uint8_t>* vec = InterpretObjectData<VectorObjectData<uint8_t>>(buf);
        	for (size_t i = 0; i < vec->size(); ++i)
        	{
        		auto pinId = GetPinId(nos::Name("Output " + std::to_string(i)));
        		if (!pinId)
        			continue;
				nosQueryBufferParams params = {};
				params.Buffer = buf;
				nosDataPathComponent path = { NOS_DATA_PATH_ARRAY_ELEMENT, i };
				params.Path = &path;
				params.PathLength = 1;
				params.TypeName = GetPin(NSN_Input)->TypeName;
				auto pinValue = QueryBuffer(params);
				if (!pinValue) {
					nosEngine.LogE("%s[%d] not found", nos::Name(params.TypeName).AsCStr(), i);
					continue;
				}
				SetPinValueCached(*pinId, *pinValue);
        	}
        	break;
        }
        case NOS_BASE_TYPE_STRUCT:
        {
			auto root = type->ByteSize ? (flatbuffers::Table*)buf.Data : InterpretObjectData<flatbuffers::Table>(buf);
            for (int i = 0; i < type->FieldCount; ++i)
            {
				auto& field = type->Fields[i];
				auto pin = GetPin(field.Name);
				if (!pin)
					continue;

				nosQueryBufferParams params = {};
				params.Buffer = buf;
				nosDataPathComponent path = {};
				path.ComponentType = NOS_DATA_PATH_FIELD_COMPONENT;
				path.Component.FieldName = field.Name;
				params.Path = &path;
				params.PathLength = 1;
				params.TypeName = GetPin(NSN_Input)->TypeName;
				auto pinValue = QueryBuffer(params);
				if (!pinValue) {
					continue;
				}
				SetPinValueCached(pin->Id, *pinValue);
            }
        }
        }
    }

    nosResult ExecuteNode(NodeExecuteParams const& params) override
	{
		if(!Type)
			return NOS_RESULT_SUCCESS;


		switch (ObjectKind)
		{
		case NOS_OBJECT_KIND_PRIMITIVE:
		{
			SetOutputValues(params.GetPinDataBuffer(NSN_Input));
			break;
		}
		case NOS_OBJECT_KIND_FOREIGN:
		{
			SetOutputValues(*SerializeObject(params.GetPinObject(NSN_Input)));
			break;
		}
		case NOS_OBJECT_KIND_ARRAY:
		{
			for (size_t i = 0; i < ArraySize; ++i)
			{
				auto pinId = GetPinId(nos::Name("Output " + std::to_string(i)));
				if (!pinId)
					continue;
				nosObjectHandle elementHandle = 0;
				if (NOS_RESULT_SUCCESS != nosEngine.ObjectAPI->GetArrayElement(params.GetPinObject(NSN_Input), i, &elementHandle))
				{
					nosEngine.LogE("Failed to get array element %d", i);
					continue;
				}
				SetPinObject(*pinId, elementHandle);
			}
			break;
		}
		case NOS_OBJECT_KIND_COMPOSITE:
		{
			auto inputObj = params.GetPinObject<CompositeObjectRef>(NSN_Input);
			for (auto& field : inputObj)
			{
				auto pin = GetPin(field.Name);
				if (!pin)
					continue;
				SetPinObject(pin->Id, field.Object);
			}
			break;
		}
		}
		params.MarkAllOutsDirty = false;
		return NOS_RESULT_SUCCESS;
	}
};

nosResult RegisterBreak(nosNodeFunctions* fn)
{ 
    NOS_BIND_NODE_CLASS(NSN_Break, BreakNode, fn);
	return NOS_RESULT_SUCCESS;
}

} // namespace nos