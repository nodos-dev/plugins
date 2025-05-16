// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include "TypeCommon.h"

// Nodos SDK
#include <PluginConfig_generated.h>
#include <nosVulkanSubsystem/Types_generated.h>

namespace nos::reflect
{
NOS_REGISTER_NAME(Type)
struct PartialUpdateTestNode : NodeContext
{
    std::optional<nos::TypeInfo> Type = {};
    nos::Name VisualizerName = {};

    nosResult OnCreate(const fb::Node* node) override
    {
        if (flatbuffers::IsFieldPresent(node, fb::Node::VT_TEMPLATE_PARAMETERS) && 1 == node->template_parameters()->size())
        {
            auto p = node->template_parameters()->Get(0);
			nos::Name typeName = nos::Name((const char*)p->value()->Data());
			Type = nos::TypeInfo(typeName);
			std::optional<std::string> updateDisplayName = std::nullopt;
			
            if(flatbuffers::IsFieldPresent(node, fb::Node::VT_DISPLAY_NAME) && node->display_name()->str().empty())
                updateDisplayName = "Make " + nos::Name(Type->TypeName).AsString();
            LoadPins(updateDisplayName ? updateDisplayName->c_str() : nullptr);
        }
		return NOS_RESULT_SUCCESS;
    }
	
	// Strict mode: only builtin types are supported
	static bool IsTypeSupported(nos::TypeInfo& info, bool strict) {
		if (info->BaseType == NOS_BASE_TYPE_NONE)
			return false;
		// If has 'skip_make' attribute
		if (info->BaseType == NOS_BASE_TYPE_STRUCT || info->BaseType == NOS_BASE_TYPE_UNION
			|| info->BaseType == NOS_BASE_TYPE_ARRAY)
		{

			bool skip = strict;
			for (int i = 0; i < info->AttributeCount; ++i)
			{
				if (info->Attributes[i].Name == NOS_NAME_STATIC("builtin") || info->Attributes[i].Name == NOS_NAME_STATIC("force_make"))
					skip = false;
				else if (info->Attributes[i].Name == NOS_NAME_STATIC("skip_make"))
				{
					skip = true;
					break;
				}
			}
			if (skip)
				return false;
		}
		return true;
	}

    void OnPinConnected(nos::Name pinName, uuid const& connectedPin) override
    {
        if (pinName == NSN_Value && Type)
        {
			if (Type->TypeName == NOS_NAME_STATIC("string"))
            {
				nosName visualizerName{};
				nosEngine.GetPinVisualizerName(connectedPin, &visualizerName);
				UpdateVisualizer(visualizerName);
			}
		}
    }

    nosResult ExecuteNode(nosNodeExecuteParams* params) override
    {
		if (!Type)
			return NOS_RESULT_SUCCESS;

		flatbuffers::FlatBufferBuilder fbb;
		NodeExecuteParams pins(params);
		auto& type = *Type;
		switch (type->BaseType)
		{
		case NOS_BASE_TYPE_FLOAT:
		case NOS_BASE_TYPE_INT:
		case NOS_BASE_TYPE_UINT:
		case NOS_BASE_TYPE_STRING:
		case NOS_BASE_TYPE_UNION:
			nosEngine.SetPinValue(pins[NSN_Output].Id, *pins[NSN_Value].Data);
			return NOS_RESULT_SUCCESS;
		}

		flatbuffers::uoffset_t offset = CopyArgs(fbb, type, pins);
		fbb.Finish(flatbuffers::Offset<flatbuffers::Vector<uint8_t>>(offset));
		nos::Buffer buf = fbb.Release();

		auto root = flatbuffers::GetRoot<flatbuffers::Table>(buf.Data());

		SetPinValue(NSN_Output, type->ByteSize ? nosBuffer{(void*)root, type->ByteSize}
											   : nosBuffer{(void*)(buf.Data()), buf.Size()});
		return NOS_RESULT_SUCCESS;
    }

	std::vector<nosName> AllTypeNames;
    
    void OnMenuRequested(nosContextMenuRequestPtr request) override
    {
        if(Type) 
            return;
		flatbuffers::FlatBufferBuilder fbb;
    	size_t count = 0;
		auto res = nosEngine.GetPinDataTypeNames(nullptr, &count);
    	if (NOS_RESULT_FAILED == res)
    		return;
		AllTypeNames.resize(count);
    	res = nosEngine.GetPinDataTypeNames(AllTypeNames.data(), &count);
    	if (NOS_RESULT_FAILED == res)
    		return;
    	std::vector<flatbuffers::Offset<nos::ContextMenuItem>> types;
    	uint32_t index = 0;
    	for (auto ty : AllTypeNames)
    		types.push_back(nos::CreateContextMenuItemDirect(fbb, nos::Name(ty).AsCStr(), index++));
    	HandleEvent(CreateAppEvent(fbb, app::CreateAppContextMenuUpdateDirect(fbb, &NodeId, request->pos(), request->instigator(), &types)));
    }

    void OnMenuCommand(uuid const& itemID, uint32_t cmd) override
    {
		if(Type) 
			return;
    	if (cmd >= AllTypeNames.size())
			return;
    	auto tyName = AllTypeNames[cmd];
    	auto typeInfo = nos::TypeInfo(tyName);
    	SetType(typeInfo);
	}

    // Set the template parameter, update pin type
    void SetType(nosTypeInfo const* typeInfo)
    {
	    // Set template parameter
        flatbuffers::FlatBufferBuilder fbb;
        
        std::vector<uint8_t> data = nos::Buffer(nos::Name(typeInfo->TypeName).AsCStr(), 1 + nos::Name(typeInfo->TypeName).AsString().size());
		std::vector<flatbuffers::Offset<fb::TemplateParameter>> params = {
			fb::CreateTemplateParameterDirect(fbb, NSN_Type.AsCStr(), "string", &data)};
        auto paramsOffset = fbb.CreateVector(params);
		auto typeNameOffset = fbb.CreateString(nos::Name(typeInfo->TypeName).AsCStr());
        
        PinResolveRequest(NSN_Output, typeInfo->TypeName);
        PartialNodeUpdateBuilder update(fbb);
        update.add_node_id(&NodeId);
        update.add_template_parameters(paramsOffset);
        HandleEvent(CreateAppEvent(fbb, update.Finish()));
    }

    void OnPinUpdated(const nosPinUpdate* update) override
    {
		if (Type)
			return;
        if (update->UpdatedField == NOS_PIN_FIELD_TYPE_NAME)
        {
			if (update->PinName != NSN_Output)
				return;
			Type = nos::TypeInfo(update->TypeName);
            LoadPins();
		}
	}

    nosResult OnResolvePinDataTypes(nosResolvePinDataTypesParams* params) override
    { 
        nos::TypeInfo incomingType(params->IncomingTypeName);
        if (!IsTypeSupported(incomingType, false))
        {
            strcpy(params->OutErrorMessage, "Type not supported for make.");
            return NOS_RESULT_FAILED;
        }
		return NOS_RESULT_SUCCESS;
    }

    void LoadPins(const char* updatedDisplayName = nullptr)
    {
		assert((*Type)->BaseType != NOS_BASE_TYPE_NONE);
		auto& type = *Type;
    	auto defBuf = GetDefaultValueOfType(type->TypeName); // TODO: This can be freed after type is unloaded, so beware.
		if (!defBuf)
			return;
    	nos::Buffer buf = defBuf->GetBuffer();
    	std::vector<uint8_t> data = buf;
        flatbuffers::FlatBufferBuilder fbb;
        std::vector<flatbuffers::Offset<nos::fb::Pin>> pinsToAdd = {};
        std::vector<::flatbuffers::Offset<PartialPinUpdate>> pinsToUpdate = {};
        std::vector<fb::UUID> pinsToDelete = {};

        std::unordered_set<nosName> pinNames = { NSN_Output };

        if (auto out = GetPin(NSN_Output))
		{
			if (out->TypeName != type->TypeName || out->IsOrphan)
			{
				pinsToUpdate.push_back(CreatePartialPinUpdateDirect(fbb,
																	&out->Id,
																	0,
																	nos::fb::CreatePinOrphanStateDirect(fbb, fb::PinOrphanStateType::ACTIVE),
																	nos::Name(type->TypeName).AsCStr(),
																	nos::Name(type->TypeName).AsCStr()));
			}
		}
		else
		{
			uuid id = nosEngine.GenerateID();
			nos::fb::TPin outPin{};
            outPin.id = id;
            outPin.name = nos::Name(NSN_Output).AsCStr();
            outPin.type_name = nos::Name(Type->TypeName).AsCStr();
            outPin.show_as = nos::fb::ShowAs::OUTPUT_PIN;
            outPin.can_show_as = nos::fb::CanShowAs::OUTPUT_PIN_ONLY;
            outPin.data = data;
            outPin.display_name = nos::Name(Type->TypeName).AsCStr();
			pinsToAdd.push_back(fb::CreatePin(fbb, &outPin));
        }

        // If the type is a primitive, then it will be constructed from a single pin named "Value"
        switch (type->BaseType)
        {
        case NOS_BASE_TYPE_INT:   
        case NOS_BASE_TYPE_UINT:  
        case NOS_BASE_TYPE_FLOAT: 
        case NOS_BASE_TYPE_STRING:
		case NOS_BASE_TYPE_UNION:
			pinNames.insert(NSN_Value);
			if (auto pin = GetPin(NSN_Value))
            {
                if (pin->IsOrphan)
                {
                    pinsToUpdate.push_back(CreatePartialPinUpdateDirect(fbb,
                        &pin->Id,
                        0,
                        nos::fb::CreatePinOrphanStateDirect(fbb, fb::PinOrphanStateType::ACTIVE),
                        nos::Name(type->TypeName).AsCStr(),
                        nos::Name(NSN_Value).AsCStr()));
                }
                if (type->BaseType == NOS_BASE_TYPE_STRING)
                {
					nosName visName{};
					nosEngine.GetPinVisualizerName(pin->Id, &visName);
                    VisualizerName = visName;
                }
            }
            else
            {
                uuid id = nosEngine.GenerateID();
                std::vector<uint8_t> data(type->ByteSize);
                if (type->BaseType == NOS_BASE_TYPE_STRING)
                {
                    data = std::vector<uint8_t>(1, 0);
                }
                pinsToAdd.push_back(fb::CreatePinDirect(fbb, &id, nos::Name(NSN_Value).AsCStr(), nos::Name(type->TypeName).AsCStr(), nos::fb::ShowAs::INPUT_PIN, nos::fb::CanShowAs::INPUT_PIN_OR_PROPERTY, 0, &data));
            }
            break;
        case NOS_BASE_TYPE_NONE: break;
        case NOS_BASE_TYPE_ARRAY: break;
        case NOS_BASE_TYPE_STRUCT:
        {
			auto rootIftable = type->ByteSize ? nullptr : buf.As<flatbuffers::Table>();
            for (int i = 0; i < type->FieldCount; ++i)
            {
                auto field = type->Fields[i];
                pinNames.insert(field.Name);
                if (auto f = GetPin(field.Name))
                {
                    if (f->TypeName != field.Type->TypeName || f->IsOrphan)
                    {
                        pinsToUpdate.push_back(CreatePartialPinUpdateDirect(fbb,
                            &f->Id,
                            0,
                            nos::fb::CreatePinOrphanStateDirect(fbb, fb::PinOrphanStateType::ACTIVE),
                            nos::Name(field.Type->TypeName).AsCStr(),
                            nos::Name(field.Name).AsCStr()));
                    }
                }
                else
                {
                    uuid id = nosEngine.GenerateID();
					std::vector<uint8_t> data;
					if (type->ByteSize)
					{
						auto* fieldStart = buf.As<uint8_t>() + field.Offset;
						data = std::vector<uint8_t>(fieldStart,
											   fieldStart + field.Type->ByteSize);
                    }
					else
					{
                        data = GenerateBuffer(field.Type, rootIftable->GetStruct<uint8_t*>(field.Offset));
                    }
                    pinsToAdd.push_back(fb::CreatePinDirect(fbb, &id, nos::Name(field.Name).AsCStr(), nos::Name(field.Type->TypeName).AsCStr(), nos::fb::ShowAs::INPUT_PIN, nos::fb::CanShowAs::INPUT_PIN_OR_PROPERTY, 0, &data));
                }
            }
        }
            break;
        }

        for (auto& [name, id]: PinName2Id )
            if (!pinNames.contains(name))
                pinsToDelete.push_back(id);

        if (!pinsToAdd.empty() ||
            !pinsToDelete.empty() ||
            !pinsToUpdate.empty())
		{
			std::vector<uint8_t> data =
				nos::Buffer(nos::Name(Type->TypeName).AsCStr(), 1 + nos::Name(Type->TypeName).AsString().size());
			std::vector<flatbuffers::Offset<fb::TemplateParameter>> params = {
				fb::CreateTemplateParameterDirect(fbb, NSN_Type.AsCStr(), "string", &data)};
			HandleEvent(CreateAppEvent(fbb,
												  CreatePartialNodeUpdateDirect(fbb,
																				&NodeId,
																				ClearFlags::CLEAR_TEMPLATE_PARAMETERS,
																				&pinsToDelete,
																				&pinsToAdd,
																				0,
																				0,
																				0,
																				0,
																				0,
																				&pinsToUpdate,
																				0,
																				0,
																				&params,
																				updatedDisplayName)));
		}
    }

	void UpdateVisualizer(nos::Name newVisualizerName)
	{
		if (Type->TypeName != NOS_NAME_STATIC("string"))
			return;
		if (newVisualizerName == VisualizerName)
			return;
		VisualizerName = newVisualizerName;
		nos::fb::TVisualizer visualizer{};
		if (VisualizerName)
		{
			visualizer.type = nos::fb::VisualizerType::COMBO_BOX;
			visualizer.name = VisualizerName.AsCStr();
		}
		else
		{
			visualizer.type = nos::fb::VisualizerType::NONE;
		}
		SetPinVisualizer(NSN_Value, visualizer);
	}

    void SetPinValuePartially() {
        auto pinId = GetPinId(NSN_Input);
        if (!pinId)
            nosEngine.LogE("Pin not found!");
        nosBuffer buf;
        if (nosEngine.GetDefaultValueOfType(nos::Name("nos.sys.vulkan.BlendMode"), &buf) != NOS_RESULT_SUCCESS) {
            nosEngine.LogE("Failed to get default value of type");
            return;
        }
        nos::sys::vulkan::TBlendMode cppPassInfo = {};
        InterpretPinValue<nos::sys::vulkan::BlendMode>(buf)->UnPackTo(&cppPassInfo);
        cppPassInfo.alpha_op = nos::sys::vulkan::BlendOp::REVERSE_SUBTRACT;
        cppPassInfo.color_op = nos::sys::vulkan::BlendOp::REVERSE_SUBTRACT;
        nos::Buffer finalBuf = nos::Buffer::From(cppPassInfo);
        std::string abc = "bc";
        nosEngine.SetPinValuePartially(*pinId, "pass1\\blend_mode", finalBuf);
    }

    static nosResult GetFunctions(size_t* count, nosName* names, nosPfnNodeFunctionExecute* fns)
    {
        *count = 1;
        if (!names || !fns)
            return NOS_RESULT_SUCCESS;

        *names = NOS_NAME_STATIC("SetPinValue");
        *fns = [](void* ctx, nosFunctionExecuteParams* params)
            {
                auto writeImage = (PartialUpdateTestNode*)ctx;
                writeImage->SetPinValuePartially();
                return NOS_RESULT_SUCCESS;
            };

        return NOS_RESULT_SUCCESS;
    }
};

nosResult RegisterPartialUpdateTest(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NSN_PartialUpdateTest, PartialUpdateTestNode, fn);
	return NOS_RESULT_SUCCESS;
}

} // namespace nos