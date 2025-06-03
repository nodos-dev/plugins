// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include "TypeCommon.h"
#include "nosReflect/Reflect_generated.h"

namespace nos::reflect
{
NOS_REGISTER_NAME(Type)
NOS_REGISTER_NAME(Operator)
std::string CapitalizeFirstLetter(const char* str)
{
	std::string copy = str;
	for (int i = 0; i < copy.length(); i++)
	{
		if (i == 0)
			copy[i] = ::toupper(str[i]);
		else if (str[i - 1] == ' ')
			copy[i] = ::toupper(str[i]);
		else
			copy[i] = str[i];
	}
	return copy;
}

std::string ToLower(const char* str)
{
	std::string s(str);
	std::transform(s.begin(), s.end(), s.begin(), ::tolower);
	return s;	
}

static void MapScalarToT(nosTypeInfo ty, auto&& f)
{
    switch(ty.BaseType)
    {
    case NOS_BASE_TYPE_FLOAT:
        switch(ty.BitWidth)
        {
            case 32: return f.template operator()<float>();
            case 64: return f.template operator()<double>();
        }
        return;
    case NOS_BASE_TYPE_INT:
        switch(ty.BitWidth)
        {
            case 8:  return f.template operator()<int8_t>();
            case 16: return f.template operator()<int16_t>();
            case 32: return f.template operator()<int32_t>();
            case 64: return f.template operator()<int64_t>();
        }
        return;
    case NOS_BASE_TYPE_UINT:
        switch(ty.BitWidth)
        {
            case 8:  return f.template operator()<uint8_t>();
            case 16: return f.template operator()<uint16_t>();
            case 32: return f.template operator()<uint32_t>();
            case 64: return f.template operator()<uint64_t>();
        }
    }
    return;
}

template<typename RightHandSide>
requires std::is_scalar_v<RightHandSide> || std::is_same_v<RightHandSide, void>
static void DoOp(reflect::BinaryOperator op, nosTypeInfo const& ty, const uint8_t* lhs, std::conditional_t<std::is_same_v<RightHandSide, void>, uint8_t, RightHandSide> const* rhs, uint8_t* dst)
{
	constexpr bool isRightHandVoid = std::is_same_v<RightHandSide, void>;
    switch(ty.BaseType)
    {
        case NOS_BASE_TYPE_STRUCT:
        {
            if (!ty.ByteSize) break;
            for (int i = 0; i < ty.FieldCount; ++i)
            {
                auto off = ty.Fields[i].Offset;
				if constexpr (isRightHandVoid)
					DoOp<RightHandSide>(op, *ty.Fields[i].Type, lhs + off, rhs + off, dst + off);
				else
					DoOp<RightHandSide>(op, *ty.Fields[i].Type, lhs + off, rhs, dst + off);
			}
        }
        case NOS_BASE_TYPE_ARRAY:
        case NOS_BASE_TYPE_STRING:
        // (TODO)
        return;
    }

    MapScalarToT(ty, [&]<class T>() -> void { 
		using RightHandSideT = std::conditional_t<isRightHandVoid, T, RightHandSide>;
        switch(op)
        {
            case reflect::BinaryOperator::ADD: *(T*)dst = *(const T*)lhs + *(RightHandSideT*)rhs; break;
            case reflect::BinaryOperator::SUB: *(T*)dst = *(const T*)lhs - *(RightHandSideT*)rhs; break;
            case reflect::BinaryOperator::MUL: *(T*)dst = *(const T*)lhs * *(RightHandSideT*)rhs; break;
            case reflect::BinaryOperator::DIV: 
            {
                if (T(0) != *(RightHandSideT*)rhs)
                    *(T*)dst = *(T*)lhs / *(RightHandSideT*)rhs;
                else nosEngine.LogW("Division by zero!");
                break;
            }
            case reflect::BinaryOperator::EXP: *(T*)dst = (T)std::pow(*(const T*)lhs, *(RightHandSideT*)rhs); break;
            case reflect::BinaryOperator::LOG: *(T*)dst = (T)(std::log(*(const T*)lhs) / std::log(*(RightHandSideT*)rhs)); break;
            default:
                *(T*)dst = 0;
        }
    });
}

void DoScalarOp(reflect::BinaryOperator op, nosTypeInfo const& ty, const uint8_t* lhs, nosTypeInfo const& scalarTy, const void* rhs, uint8_t* dst)
{
	MapScalarToT(scalarTy, [&]<class T>() -> void {
		DoOp<T>(op, ty, lhs, static_cast<const T*>(rhs), dst);
	});
}

const char* BinaryOpToDisplayName(reflect::BinaryOperator op)
{
	static std::unordered_map<reflect::BinaryOperator, std::string> names;
	auto it = names.find(op);
	if (it == names.end())
		names[op] = CapitalizeFirstLetter(ToLower(reflect::EnumNameBinaryOperator(op)).c_str());
	return names[op].c_str();
}

template<bool IsScalarArithmetic>
struct ArithmeticNodeContext : NodeContext
{
	std::optional<nos::TypeInfo> Type = std::nullopt;
	std::optional<reflect::BinaryOperator> Operator;
	std::conditional_t<IsScalarArithmetic, std::optional<nos::TypeInfo>, std::tuple<>> ScalarType =
		[]() {
		if constexpr (IsScalarArithmetic)
			return std::nullopt;
		else
			return std::tuple<>{};
		}();

	nosResult OnCreate(const fb::Node* node) override
	{
		std::optional<nosName> newTypeName;

		if (flatbuffers::IsFieldPresent(node, fb::Node::VT_TEMPLATE_PARAMETERS))
		{
			for (auto templateParam : *node->template_parameters())
			{
				if ("string" == templateParam->name()->string_view() ||
					NSN_Type == templateParam->name()->string_view())
				{
					newTypeName = nos::Name((const char*)(templateParam->value()->Data()));
					continue;
				}
				if ("nos.reflect.BinaryOperator" == templateParam->name()->string_view() ||
					NSN_Operator == templateParam->name()->string_view())
				{
					Operator = *InterpretObjectData<reflect::BinaryOperator>((const void*)templateParam->value()->Data());
					continue;
				}
			}
		}
		if (newTypeName && *newTypeName != NSN_TypeNameGeneric) {
			Type = nos::TypeInfo(*newTypeName);
			if (node->class_name()->str() == "nos.reflect." + NSN_ArithmeticDynamic.AsString()) {
				if (node->pins() && node->pins()->size())
					ResolvePinsForArithmeticDynamic(node);
				else
					CreatePins();
			}
		}
		if constexpr (IsScalarArithmetic)
		{
			std::optional<nos::Name> scalarTypeName;
			for (auto* pin : *node->pins())
			{
				if (pin->name()->string_view() == NSN_Scalar)
				{
					scalarTypeName = nos::Name(pin->type_name()->str());
					break;
				}
			}

			if (scalarTypeName && *scalarTypeName != NSN_TypeNameGeneric)
				ScalarType = nos::TypeInfo(*scalarTypeName);
		}

		if (Operator)
			SetOperator(*Operator, false);
		return NOS_RESULT_SUCCESS;
	}

	void ResolvePinsForArithmeticDynamic(const fb::Node* node) {
		flatbuffers::FlatBufferBuilder fbb;
		std::vector<::flatbuffers::Offset<PartialPinUpdate>> pinsToUpdate = {};
		for (auto pin : *node->pins()) {
			pinsToUpdate.push_back(CreatePartialPinUpdateDirect(fbb, pin->id(), nullptr, nos::fb::CreatePinOrphanStateDirect(fbb)));
		}
		HandleEvent(CreateAppEvent(fbb,
			CreatePartialNodeUpdateDirect(fbb,
				&NodeId,
				ClearFlags::NONE,
				nullptr,
				nullptr,
				nullptr,
				nullptr,
				nullptr,
				nullptr,
				nullptr,
				&pinsToUpdate)));

	}

	void CreatePins() {
		auto& type = *Type;
		auto defBuf = GetDefaultValueOfType(type->TypeName); // TODO: This can be freed after type is unloaded, so beware.
		if (!defBuf)
			return;
		nos::Buffer buf = defBuf->GetBuffer();
		std::vector<uint8_t> data = buf;
		flatbuffers::FlatBufferBuilder fbb;
		std::vector<flatbuffers::Offset<nos::fb::Pin>> pinsToAdd = {};

		std::vector<nosName> pinNames = { NOS_NAME("A"), NOS_NAME("B"), NOS_NAME("Output")};
		std::vector<nos::fb::ShowAs> pinShowAs = { nos::fb::ShowAs::INPUT_PIN, nos::fb::ShowAs::INPUT_PIN, nos::fb::ShowAs::OUTPUT_PIN };
		std::vector<nos::fb::CanShowAs> pinCanShowAs = { nos::fb::CanShowAs::INPUT_PIN_OR_PROPERTY, nos::fb::CanShowAs::INPUT_PIN_OR_PROPERTY, nos::fb::CanShowAs::OUTPUT_PIN_ONLY };
		for (uint32_t pinIndx = 0; pinIndx < pinNames.size(); pinIndx++) {
			nos::fb::TPin pin{};
			uuid id = nosEngine.GenerateID();
			pin.id = id;
			pin.name = nos::Name(pinNames[pinIndx]).AsCStr();
			pin.type_name = nos::Name(Type->TypeName).AsCStr();
			pin.show_as = pinShowAs[pinIndx];
			pin.can_show_as = pinCanShowAs[pinIndx];
			pin.data = data;
			pin.display_name = nos::Name(pinNames[pinIndx]).AsCStr();
			pinsToAdd.push_back(fb::CreatePin(fbb, &pin));
		}

		HandleEvent(CreateAppEvent(
			fbb,
			CreatePartialNodeUpdateDirect(fbb, &NodeId, nos::ClearFlags::NONE, nullptr, &pinsToAdd)));
	}

	void SetOperator(reflect::BinaryOperator op, bool addTemplateParams)
	{
		Operator = op;
		flatbuffers::FlatBufferBuilder fbb;
		std::string displayNameStr = BinaryOpToDisplayName(op);
		if constexpr (IsScalarArithmetic)
			displayNameStr += " (Scalar)";
		auto displayName = fbb.CreateString(displayNameStr);
		std::vector<uint8_t> opData = PackObjectData(*Operator);
		std::vector params = {
			fb::CreateTemplateParameterDirect(fbb, NSN_Operator.AsCStr(), "nos.reflect.BinaryOperator", &opData)
		};
		auto templateParamsOffset = fbb.CreateVector(params);
		PartialNodeUpdateBuilder update(fbb);
		update.add_node_id(&NodeId);
		if (addTemplateParams)
			update.add_template_parameters(templateParamsOffset);
		update.add_display_name(displayName);
		HandleEvent(CreateAppEvent(fbb, update.Finish()));
	}

	bool IsTypeCompatible(TypeInfo& typeInfo)
	{
		if (typeInfo->BaseType == NOS_BASE_TYPE_STRUCT && !typeInfo->ByteSize)
			return false;
		if (typeInfo->BaseType == NOS_BASE_TYPE_ARRAY || typeInfo->BaseType == NOS_BASE_TYPE_UNION)
			return false;

		if (Operator != BinaryOperator::ADD && typeInfo->BaseType == NOS_BASE_TYPE_STRING)
			return false;

		if (typeInfo->AttributeCount == 0)
			return false;
		for (int i = 0; i < typeInfo->AttributeCount; ++i)
			if (typeInfo->Attributes[i].Name == NOS_NAME_STATIC("resource"))
				return false;
		return true;
	}

	void SetType(nosName typeName)
	{
		Type = nos::TypeInfo(typeName);
		flatbuffers::FlatBufferBuilder fbb;
		std::vector<uint8_t> typeData = nos::Buffer(nos::Name(Type->TypeName).AsCStr(), 1 + nos::Name(Type->TypeName).AsString().size());
		std::vector params = {fb::CreateTemplateParameterDirect(fbb, NSN_Type.AsCStr(), "string", &typeData)
		};
		auto templateParamsOffset = fbb.CreateVector(params);
		PartialNodeUpdateBuilder update(fbb);
		update.add_node_id(&NodeId);
		update.add_template_parameters(templateParamsOffset);
		HandleEvent(CreateAppEvent(fbb, update.Finish()));
		if (!IsTypeCompatible(*Type))
		{
			// Make pins orphan
			SetPinOrphanState(NSN_A, nos::fb::PinOrphanStateType::ORPHAN, "Type not supported.");
			if constexpr (!IsScalarArithmetic)
				SetPinOrphanState(NSN_B, nos::fb::PinOrphanStateType::ORPHAN, "Type not supported.");
			SetPinOrphanState(NSN_Output, nos::fb::PinOrphanStateType::ORPHAN, "Type not supported.");
		}
	}

	void SetScalarType(nosName typeName)
	{
		static_assert(IsScalarArithmetic);
		ScalarType = nos::TypeInfo(typeName);
	}

	void OnPinUpdated(const nosPinUpdate* update) override
	{
		if (update->UpdatedField != NOS_PIN_FIELD_TYPE_NAME)
			return;
		bool isScalarPin = update->PinName == NSN_Scalar;
		if constexpr (IsScalarArithmetic)
			if (isScalarPin)
			{
				if (!ScalarType || ScalarType->TypeName != update->TypeName)
					SetScalarType(update->TypeName);
				return;
			}
		if (!Type || Type->TypeName != update->TypeName)
			SetType(update->TypeName);
	}

	nosResult OnResolvePinDataTypes(nosResolvePinDataTypesParams* params) override
	{
		nos::TypeInfo incomingType(params->IncomingTypeName);
		if constexpr (IsScalarArithmetic)
		{
			bool isScalarPin = params->InstigatorPinName == NSN_Scalar;
			for (size_t i = 0; i < params->PinCount; i++)
			{
				auto& pin = params->Pins[i];
				if ((pin.Name == NSN_Scalar) ^ isScalarPin)
					pin.OutResolvedTypeName = NSN_TypeNameGeneric;
			}
			if (isScalarPin)
			{
				bool isTypeScalar = incomingType->BaseType == NOS_BASE_TYPE_INT || incomingType->BaseType == NOS_BASE_TYPE_UINT || incomingType->BaseType == NOS_BASE_TYPE_FLOAT;
				if (!isTypeScalar)
				{
					strcpy(params->OutErrorMessage, "Scalar arithmetic requires a scalar type");
					return NOS_RESULT_FAILED;
				}
				return NOS_RESULT_SUCCESS;
			}
		}
		/*
		* This part is disabled since Interpolate node from nos.Animation uses this node in its path for non-suitable types, but it is not executed. Fix it gracefully later.
		if (incomingType->BaseType == NOS_BASE_TYPE_STRUCT && !incomingType->ByteSize)
			return NOS_RESULT_FAILED;
		if (incomingType->BaseType == NOS_BASE_TYPE_ARRAY || incomingType->BaseType == NOS_BASE_TYPE_UNION)
			return NOS_RESULT_FAILED;

		if (Operator != BinaryOperator::ADD && incomingType->BaseType == NOS_BASE_TYPE_STRING)
			return NOS_RESULT_FAILED;

		if (incomingType->AttributeCount == 0)
			return NOS_RESULT_SUCCESS;
		for (int i = 0; i < incomingType->AttributeCount; ++i)
		{
			if (incomingType->Attributes[i].Name == NOS_NAME_STATIC("resource"))
			{
				strcpy(params->OutErrorMessage, "Resource types are not supported");
				return NOS_RESULT_FAILED;
			}
		}
		*/
		return NOS_RESULT_SUCCESS;
	}

	nosResult ExecuteNode(NodeExecuteParams const& params) override
	{
		if (!Type || NSN_TypeNameGeneric == Type->TypeName || !Operator)
			return NOS_RESULT_SUCCESS;
		if constexpr (IsScalarArithmetic)
		{
			if (!ScalarType || NSN_TypeNameGeneric == ScalarType->TypeName)
				return NOS_RESULT_SUCCESS;
		}

		flatbuffers::FlatBufferBuilder fbb;

		if constexpr (!IsScalarArithmetic)
		{
			auto& B = params[NSN_B];
			// TODO: we can directly set execute node function ptr instead of switch case etc.
			if ((*Type)->BaseType == NOS_BASE_TYPE_STRING)
			{
				std::stringstream ss;
				ss << params.GetPinValue<const char*>(NSN_A) << params.GetPinValue<const char*>(NSN_B);
				auto str = ss.str();
				SetPinValue(NSN_Output, str.c_str());
			}
			else
			{
				nos::Buffer outputBuf = params.GetPinBuffer(NSN_Output);
				DoOp<void>(*Operator,
						   *Type,
						   params.GetPinValue<uint8_t>(NSN_A),
						   params.GetPinValue<uint8_t>(NSN_B),
						   outputBuf.As<uint8_t>());
				SetPinValue(NSN_Output, outputBuf);
			}
		}
		else
		{
			nos::Buffer outputBuf = params.GetPinBuffer(NSN_Output);
			DoScalarOp(*Operator,
					   *Type,
					   params.GetPinValue<uint8_t>(NSN_A),
					   *ScalarType,
					   params.GetPinValue<void>(NSN_Scalar),
					   outputBuf.As<uint8_t>());
			SetPinValue(NSN_Output, outputBuf);
		}

		return NOS_RESULT_SUCCESS;
	}

	void OnMenuRequested(nosContextMenuRequestPtr request) override
	{
		if (Operator)
			return;
		flatbuffers::FlatBufferBuilder fbb;
		std::vector<flatbuffers::Offset<nos::ContextMenuItem>> ops;

		for (auto op : reflect::EnumValuesBinaryOperator())
			ops.push_back(nos::CreateContextMenuItemDirect(fbb, BinaryOpToDisplayName(op), uint32_t(op)));

		HandleEvent(CreateAppEvent(fbb, CreateAppContextMenuUpdateDirect(fbb, &NodeId, request->pos(), request->instigator(), &ops)));
	}

	void OnMenuCommand(uuid const& itemId, uint32_t cmd) override
	{
		if (Operator)
			return;
		SetOperator(reflect::BinaryOperator(cmd), true);
	}

	static nosResult MigrateNode(nosFbNodePtr nodePtr, nosBuffer* outBuffer)
	{
		fb::TNode tNode;
		nodePtr->UnPackTo(&tNode);
		std::optional<reflect::BinaryOperator> op{};
		if (tNode.class_name.starts_with("nos.math.Add_"))
			op = reflect::BinaryOperator::ADD;
		else if (tNode.class_name.starts_with("nos.math.Sub_"))
			op = reflect::BinaryOperator::SUB;
		else if (tNode.class_name.starts_with("nos.math.Mul_"))
			op = reflect::BinaryOperator::MUL;
		else if (tNode.class_name.starts_with("nos.math.Div_"))
			op = reflect::BinaryOperator::DIV;
		// Dont migrate
		if (!op)
			return NOS_RESULT_SUCCESS;
		auto& templateParam = tNode.template_parameters.emplace_back(std::make_unique<fb::TTemplateParameter>());
		templateParam->name = NSN_Operator.AsString();
		templateParam->value = nos::Buffer::From(op);

		templateParam->type_name = "nos.reflect.BinaryOperator";
		tNode.class_name = "nos.reflect." + NSN_ArithmeticDynamic.AsString();
		std::string type = "float";
		for (auto& pin : tNode.pins)
		{
			type = pin->type_name;
			if (pin->name == "X")
				pin->name = NSN_A.AsString();
			else if (pin->name == "Y")
				pin->name = NSN_B.AsString();
			else if (pin->name == "Z")
				pin->name = NSN_Output.AsString();
		}
		auto& templateParam2 = tNode.template_parameters.emplace_back(std::make_unique<fb::TTemplateParameter>());
		templateParam2->name = NSN_Type.AsString();
		templateParam2->value = std::vector<uint8_t>(PackObjectData(type.c_str()));
		templateParam2->type_name = "string";
		*outBuffer = EngineBuffer::CopyFrom(tNode).Release();
		return NOS_RESULT_SUCCESS;
	}
};

void RegisterArithmeticNodePresets() {
	std::vector<nosName> typeNames;
	size_t count = 0;
	auto res = nosEngine.GetPinDataTypeNames(0, &count);
	if (NOS_RESULT_FAILED != res)
	{
		typeNames.resize(count);
		nosEngine.GetPinDataTypeNames(typeNames.data(), &count);
	}
	std::vector<nos::Buffer> nodePresets;
	for (uint32_t binaryOpIndx = 0; binaryOpIndx < uint32_t(BinaryOperator::MAX) + 1; binaryOpIndx++) {
		auto binaryOp = BinaryOperator(binaryOpIndx);
		std::string binaryOpStr = BinaryOpToDisplayName(binaryOp);
		for (auto& typeName : typeNames)
		{
			nos::TypeInfo typeInfo(typeName);
			if (typeInfo->BaseType == NOS_BASE_TYPE_NONE)
				continue;
			// Don't show table types
			if (typeInfo->BaseType == NOS_BASE_TYPE_STRUCT) {
				if (!typeInfo->ByteSize)
					continue;
				// Don't show structs with no builtin attribute
				bool skip = true;
				for (int i = 0; i < typeInfo->AttributeCount; ++i)
				{
					if (typeInfo->Attributes[i].Name == NOS_NAME_STATIC("builtin"))
						skip = false;
				}
				if (skip)
					continue;
			}
			// Don't show enums and arrays
			if (typeInfo->BaseType == NOS_BASE_TYPE_UNION || typeInfo->BaseType == NOS_BASE_TYPE_ARRAY ||
				IsEnumType(typeInfo))
				continue;

			if (binaryOp != BinaryOperator::ADD && typeInfo->BaseType == NOS_BASE_TYPE_STRING)
				continue;

			std::string name = nos::Name(typeInfo.TypeName).AsString();
			auto idx = name.find_last_of(".");
			idx = idx == std::string::npos ? 0 : 1 + idx;
			fb::TNodePreset preset;
			fb::TNodeMenuInfo info;
			info.category = "Math|" + binaryOpStr;
			info.display_name = binaryOpStr + " " + name.substr(idx);
			preset.menu_info = std::make_unique<fb::TNodeMenuInfo>(std::move(info));
			std::vector<uint8_t> data = PackObjectData(name.c_str());
			preset.params.emplace_back(new fb::TTemplateParameter{ {}, NSN_Type.AsString(), "string", std::move(data) });
			preset.params.emplace_back(new fb::TTemplateParameter{
				{}, NSN_Operator.AsString(), "nos.reflect.BinaryOperator", PackObjectData(binaryOp)});
			flatbuffers::FlatBufferBuilder fbb;
			fbb.Finish(CreateNodePreset(fbb, &preset));
			nos::Buffer buf = fbb.Release();
			nodePresets.push_back(std::move(buf));
		}
	}
	std::vector<nosFbNodePresetPtr> fbNodePresets;
	for (auto& buf : nodePresets)
		fbNodePresets.push_back(flatbuffers::GetMutableRoot<nos::fb::NodePreset>(buf.Data()));
	nosEngine.RegisterNodePresets(nos::Name("nos.reflect." + NSN_ArithmeticDynamic.AsString()), fbNodePresets.size(), fbNodePresets.data());
}

nosResult RegisterArithmetic(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NSN_Arithmetic, ArithmeticNodeContext<false>, fn);
	return NOS_RESULT_SUCCESS;
}

nosResult RegisterArithmeticDynamic(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NSN_ArithmeticDynamic, ArithmeticNodeContext<false>, fn);
	RegisterArithmeticNodePresets();
	return NOS_RESULT_SUCCESS;
}

nosResult RegisterScalarArithmetic(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NSN_ScalarArithmetic, ArithmeticNodeContext<true>, fn);
	return NOS_RESULT_SUCCESS;
}

} // namespace nos