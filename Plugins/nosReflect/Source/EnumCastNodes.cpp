// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include "TypeCommon.h"

namespace nos::reflect
{
NOS_REGISTER_NAME(Enum)

bool IsPrimitiveType(nosTypeInfo const& typeInfo)
{
	switch (typeInfo.BaseType)
	{
	case NOS_BASE_TYPE_INT:
	case NOS_BASE_TYPE_UINT:
	case NOS_BASE_TYPE_FLOAT:
		return true;
	default:
		return false;
	}
}

const char* PrimitiveToString(nosTypeInfo const& typeInfo)
{
	switch (typeInfo.BaseType)
	{
	case NOS_BASE_TYPE_NONE: return "None";
	case NOS_BASE_TYPE_INT: return typeInfo.ByteSize == 4 ? "int" : "long";
	case NOS_BASE_TYPE_UINT: return typeInfo.ByteSize == 4 ? "uint" : "ulong";
	case NOS_BASE_TYPE_FLOAT: return typeInfo.ByteSize == 4 ? "float" : "double";
	default: return "Unknown";
	}
}


struct EnumCastNodeBase : NodeContext
{
	std::optional<nos::TypeInfo> EnumType = std::nullopt;

	EnumCastNodeBase(nosFbNodePtr node) : NodeContext(node)
	{
		for (auto* pin : *node->pins())
		{
			if (pin->name()->string_view() != NSN_Enum)
				continue;
			auto typeName = nos::Name(pin->type_name()->string_view());
			if (NSN_TypeNameGeneric == typeName)
				break;
			auto ty = nos::TypeInfo(typeName);
			if (!ty)
				return;
			EnumType = std::move(ty);
			break;
		}
	}

	void OnPinUpdated(const nosPinUpdate* update) override
	{
		if (EnumType)
			return;
		if (update->UpdatedField == NOS_PIN_FIELD_TYPE_NAME)
		{
			if (update->PinName != NSN_Enum)
				return;
			EnumType = nos::TypeInfo(update->TypeName);
		}
	}

	nosResult OnResolvePinDataTypes(nosResolvePinDataTypesParams* params) override
	{
		auto info = nos::TypeInfo(params->IncomingTypeName);
		if (params->InstigatorPinName != NSN_Enum)
		{
			strcpy(params->OutErrorMessage, "Type can only be decided by the Enum pin.");
			return NOS_RESULT_FAILED;
		}
		if (!IsPrimitiveType(*info) || PrimitiveToString(*info) == nos::Name(info->TypeName))
		{
			strcpy(params->OutErrorMessage, "Only enum types can be connected to the Enum pin.");
			return NOS_RESULT_FAILED;
		}
		for (size_t i = 0; i < params->PinCount; ++i)
		{
			auto& pin = params->Pins[i];
			if (pin.Id == uuid(params->InstigatorPinId))
				continue;
			pin.OutResolvedTypeName = nos::Name(PrimitiveToString(*info));
		}
		return NOS_RESULT_SUCCESS;
	};
};

struct EnumToUnderlyingValueNode : EnumCastNodeBase
{
	using EnumCastNodeBase::EnumCastNodeBase;
	
    nosResult ExecuteNode(nosNodeExecuteParams* params) override
	{
		nos::NodeExecuteParams args(params);
		if(!EnumType || args[NSN_Value].TypeName == NSN_TypeNameGeneric)
			return NOS_RESULT_SUCCESS;
		SetPinValue(NSN_Value, *args[NSN_Enum].Data);
		return NOS_RESULT_SUCCESS;
	}
};

nosResult RegisterEnumToUnderlyingValue(nosNodeFunctions* fn)
{ 
    NOS_BIND_NODE_CLASS(NOS_NAME("EnumToUnderlyingValue"), EnumToUnderlyingValueNode, fn);
	return NOS_RESULT_SUCCESS;
}

struct EnumFromUnderlyingValueNode : EnumCastNodeBase
{
	using EnumCastNodeBase::EnumCastNodeBase;

	nosResult ExecuteNode(nosNodeExecuteParams* params) override
	{
		nos::NodeExecuteParams args(params);
		if (!EnumType || args[NSN_Value].TypeName == NSN_TypeNameGeneric)
			return NOS_RESULT_SUCCESS;
		SetPinValue(NSN_Enum, *args[NSN_Value].Data);
		return NOS_RESULT_SUCCESS;
	}
};

nosResult RegisterEnumFromUnderlyingValue(nosNodeFunctions* fn)
{ 
    NOS_BIND_NODE_CLASS(NOS_NAME("EnumFromUnderlyingValue"), EnumFromUnderlyingValueNode, fn);
	return NOS_RESULT_SUCCESS;
}

} // namespace nos