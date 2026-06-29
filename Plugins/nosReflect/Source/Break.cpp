// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include "TypeCommon.h"

#include <unordered_map>
#include <unordered_set>

namespace nos::reflect
{
// Break: decomposes a value into its parts. For a struct it exposes one output pin per field; for an
// array, one "Output N" pin per element; for a nos.Dict, one output pin per entry inferred from the
// connected dict at runtime (folded-in Break Dict behaviour). The type is resolved from the Input pin -
// either pre-set by a preset (e.g. Break Dict) or adopted when a value is connected.
struct BreakNode : NodeContext
{
	std::optional<nos::TypeInfo> Type = std::nullopt;
	nosObjectKind ObjectKind = NOS_OBJECT_KIND_PRIMITIVE;
	size_t ArraySize = 0;

	// Dict mode: managed output pins keyed by dict key. Tracked locally (rather than read back from the
	// pin map) because pin create/delete events are processed asynchronously, so RebuildDictOutputs stays
	// idempotent even when it fires several times in one tick.
	struct DictOutput
	{
		uuid Id;
		nos::Name Type;
	};
	std::unordered_map<nos::Name, DictOutput> DictOutputs;

	enum MenuCommand : uint32_t
	{
		BREAK_DICT = 1, // turn an unresolved Break into a dict breaker
	};

	bool IsDict() const { return Type && Type->TypeName == NSN_DictTypeName; }

	void OnNodeMenuRequested(nosContextMenuRequestPtr request) override
	{
		if (Type)
			return; // already resolved to a concrete type
		SendContextMenu(request, [](ContextMenuBuilder& menu) { menu.Item("Break Dictionary", BREAK_DICT); });
	}

	void OnMenuCommand(uuid const& itemId, uint32_t cmd) override
	{
		if (cmd == BREAK_DICT)
			PinResolveRequest(NSN_Input, NSN_DictTypeName);
	}

	void OnTypeUpdated(nos::Name typeName, bool preserveDisplayNames)
	{
		if (typeName == NSN_TypeNameGeneric)
		{
			Type = std::nullopt;
			return;
		}
		Type = nos::TypeInfo(typeName);
		if (NOS_RESULT_SUCCESS != nosEngine.ObjectAPI->GetObjectKindFromTypeName(typeName, &ObjectKind))
		{
			SetNodeOrphanState(fb::NodeOrphanStateType::ORPHAN, "Invalid type");
			return;
		}
		if (IsDict())
			return; // dict outputs are inferred from the connected object, not from a schema
		LoadPins(preserveDisplayNames);
	}

	nosResult OnCreate(nosFbNodePtr node) override
	{
		// A preset (e.g. Break Dict) pins the type via a template parameter; otherwise it comes from the
		// Input pin - a generic Break resolves it on connection, and saved graphs restore it here.
		bool fromTemplate = flatbuffers::IsFieldPresent(node, fb::Node::VT_TEMPLATE_PARAMETERS) &&
							 1 == node->template_parameters()->size();
		nos::Name typeName = NSN_TypeNameGeneric;
		if (fromTemplate)
			typeName = nos::Name((const char*)node->template_parameters()->Get(0)->value()->Data());
		else if (auto* in = GetPin(NSN_Input))
			typeName = in->TypeName;

		if (typeName == NSN_TypeNameGeneric)
			return NOS_RESULT_SUCCESS;
		if (!nos::TypeInfo(typeName))
			return NOS_RESULT_FAILED; // has a type name but it is not a valid type

		if (fromTemplate)
			SetPinType(NSN_Input, typeName); // retype the (generic) preset Input pin to match
		OnTypeUpdated(typeName, true);
		if (IsDict())
			RestoreDictOutputs(node);
		return NOS_RESULT_SUCCESS;
	}

	// Restore managed dict outputs from the pins persisted in a saved graph, reactivating any that came
	// back orphaned/passive (a la nos.math.Eval).
	void RestoreDictOutputs(nosFbNodePtr node)
	{
		std::vector<uuid> toUnorphan;
		for (auto* pin : *node->pins())
		{
			nos::Name name(pin->name()->string_view());
			if (pin->show_as() != fb::ShowAs::OUTPUT_PIN || name == NSN_Input)
				continue;
			DictOutputs[name] = {*pin->id(), nos::Name(pin->type_name()->string_view())};
			if (auto orphan = pin->orphan_state())
				if (orphan->type() == fb::PinOrphanStateType::PASSIVE || orphan->type() == fb::PinOrphanStateType::ORPHAN)
					toUnorphan.push_back(*pin->id());
		}
		for (auto const& id : toUnorphan)
			SetPinOrphanState(id, fb::PinOrphanStateType::ACTIVE);
	}

	void OnPinObjectChanged(nos::Name pinName, uuid const& pinId, nosObjectId newHandle) override
	{
		if (!Type || pinName != NSN_Input)
			return;
		if (IsDict())
		{
			if (newHandle.Value)
				RebuildDictOutputs(DictRef::FromObjectId(newHandle));
			return;
		}
		if ((*Type)->BaseType != NOS_BASE_TYPE_ARRAY)
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

	// On connection, if the upstream's object is already available, rebuild the dict outputs immediately.
	// Otherwise the value hasn't been produced yet and we cannot force it: a Break with no downstream
	// consumer has no thread pin, so it is not part of any scheduled path and never runs on its own. It
	// resolves once it is on a consumer's path (OnPinObjectChanged).
	//
	// TODO: Engine support needed. The engine only schedules sink nodes (those reachable on a thread path).
	// To resolve a disconnected Break (no consumer yet), the engine should be able to schedule a non-sink /
	// pathless node on demand - fulfilling SendScheduleRequest by running that node and pulling its inputs
	// once. With that, this handler could simply request a schedule.
	void OnPinConnected(nos::Name pinName, uuid const& connectedPin, nosObjectId connectedObject) override
	{
		if (pinName == NSN_Input && IsDict() && connectedObject)
			RebuildDictOutputs(DictRef::FromObjectId(connectedObject));
	}

	void OnPinUpdated(const nosPinUpdate* update) override
	{
		if (Type)
			return;
		if (update->UpdatedField == NOS_PIN_FIELD_TYPE_NAME && update->PinName == NSN_Input)
			OnTypeUpdated(update->TypeName, false);
	}

	nosResult OnResolvePinDataTypes(nosResolvePinDataTypesParams* params) override
	{
		auto info = nos::TypeInfo(params->IncomingTypeName);
		switch (info->BaseType)
		{
		case NOS_BASE_TYPE_ARRAY:
		case NOS_BASE_TYPE_STRUCT:
			for (int i = 0; i < info->AttributeCount; ++i)
				if (info->Attributes[i].Name == NOS_NAME_STATIC("skip_break"))
					return NOS_RESULT_FAILED;
			return NOS_RESULT_SUCCESS;
		default:
			return NOS_RESULT_FAILED;
		}
	}

	// Build/refresh output pins for a fixed-schema struct or array type.
	void LoadPins(bool preserveDisplayNames)
	{
		auto& type = *Type;
		std::unordered_set<nos::Name> accepted{NSN_Input};

		if (auto* in = GetPin(NSN_Input); in && in->IsOrphan)
			SetPinOrphanState(in->Id, fb::PinOrphanStateType::ACTIVE);

		if (type->BaseType == NOS_BASE_TYPE_STRUCT)
		{
			for (int i = 0; i < type->FieldCount; ++i)
			{
				auto& field = type->Fields[i];
				accepted.insert(field.Name);
				if (auto* pin = GetPin(field.Name))
				{
					if (pin->TypeName == field.Type->TypeName)
					{
						if (pin->IsOrphan)
							SetPinOrphanState(pin->Id, fb::PinOrphanStateType::ACTIVE);
						continue;
					}
					DeletePin(pin->Id); // field type changed: recreate with the new type
				}
				AddPin(PinBuilder(field.Name)
						   .SetDisplayName(field.Name)
						   .SetTypeName(field.Type->TypeName)
						   .SetShowAs(fb::ShowAs::OUTPUT_PIN)
						   .SetCanShowAs(fb::CanShowAs::OUTPUT_PIN_OR_PROPERTY)
						   .SetValue(nos::Buffer(field.DefaultValue.Data, field.DefaultValue.Size)));
			}
		}
		else if (type->BaseType == NOS_BASE_TYPE_ARRAY)
		{
			size_t i = 0;
			while (auto* pin = GetPin(nos::Name("Output " + std::to_string(i))))
			{
				accepted.insert(pin->Name);
				if (pin->IsOrphan)
					SetPinOrphanState(pin->Id, fb::PinOrphanStateType::ACTIVE);
				i++;
			}
			ArraySize = i;
		}

		for (auto& [id, pin] : Pins)
			if (!accepted.contains(pin.Name))
				DeletePin(id);

		if (!preserveDisplayNames)
			SetNodeDisplayName("Break " + nos::Name(type->TypeName).AsString());
	}

	// Grow/shrink the "Output N" pins to match the connected array's element count.
	void SetOutputCount()
	{
		auto& type = *Type;
		size_t existing = 0;
		while (auto* pin = GetPin(nos::Name("Output " + std::to_string(existing))))
		{
			if (existing >= ArraySize)
				DeletePin(pin->Id);
			existing++;
		}
		nos::Buffer elementDefault;
		if (auto buf = GetDefaultValueOfType(type->ElementType->TypeName))
			elementDefault = nos::Buffer(buf->Data(), buf->Size());
		for (size_t i = existing; i < ArraySize; i++)
		{
			auto name = nos::Name("Output " + std::to_string(i));
			AddPin(PinBuilder(name)
					   .SetDisplayName(name)
					   .SetTypeName(type->ElementType->TypeName)
					   .SetShowAs(fb::ShowAs::OUTPUT_PIN)
					   .SetCanShowAs(fb::CanShowAs::OUTPUT_PIN_OR_PROPERTY)
					   .SetValue(elementDefault));
		}
	}

	// Recreate dict output pins to match the connected dict's entries (key + per-entry concrete type).
	void RebuildDictOutputs(DictRef const& dict)
	{
		if (!dict.IsValid())
			return;
		std::unordered_set<nos::Name> desired;
		for (auto& field : dict)
		{
			desired.insert(field.Name);
			nos::Name type = field.Object.GetTypeName();
			auto it = DictOutputs.find(field.Name);
			if (it != DictOutputs.end())
			{
				if (it->second.Type == type)
					continue; // already have an output pin of the right type
				DeletePin(it->second.Id); // value type changed: recreate with the new type
				DictOutputs.erase(it);
			}
			uuid id = nosEngine.GenerateID();
			AddPin(PinBuilder(field.Name)
					   .SetId(id)
					   .SetDisplayName(field.Name)
					   .SetTypeName(type)
					   .SetShowAs(fb::ShowAs::OUTPUT_PIN)
					   .SetCanShowAs(fb::CanShowAs::OUTPUT_PIN_OR_PROPERTY));
			DictOutputs[field.Name] = {id, type};
		}
		for (auto it = DictOutputs.begin(); it != DictOutputs.end();)
		{
			if (desired.contains(it->first))
				++it;
			else
			{
				DeletePin(it->second.Id);
				it = DictOutputs.erase(it);
			}
		}
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

	void SetOutputValues(nosImmutableBuffer buf)
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
				nosDataPathComponent path = {NOS_DATA_PATH_ARRAY_ELEMENT, i};
				params.Path = &path;
				params.PathLength = 1;
				params.TypeName = GetPin(NSN_Input)->TypeName;
				auto pinValue = QueryBuffer(params);
				if (!pinValue)
				{
					nosEngine.LogE("%s[%d] not found", nos::Name(params.TypeName).AsCStr(), i);
					continue;
				}
				SetPinValueCached(*pinId, *pinValue);
			}
			break;
		}
		case NOS_BASE_TYPE_STRUCT: {
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
				if (!pinValue)
					continue;
				SetPinValueCached(pin->Id, *pinValue);
			}
		}
		}
	}

	std::unordered_map<nos::Name, uint64_t> LastServedObjectId; // dict key -> last object id pushed

	nosResult ExecuteNode(NodeExecuteParams const& params) override
	{
		if (!Type)
			return NOS_RESULT_SUCCESS;

		if (IsDict())
		{
			auto dict = params.GetPinObject<DictRef>(NSN_Input);
			if (!dict.IsValid())
				return NOS_RESULT_SUCCESS;
			// Only push an entry to its output pin when that entry's object actually changed.
			for (auto& field : dict)
			{
				auto* pin = GetPin(field.Name);
				if (!pin)
					continue;
				uint64_t id = field.Object.GetObjectId().Value;
				auto it = LastServedObjectId.find(field.Name);
				if (it != LastServedObjectId.end() && it->second == id)
					continue;
				SetPinObject(pin->Id, field.Object);
				LastServedObjectId[field.Name] = id;
			}
			params.MarkAllOutsDirty = false;
			return NOS_RESULT_SUCCESS;
		}

		switch (ObjectKind)
		{
		case NOS_OBJECT_KIND_PRIMITIVE:
		case NOS_OBJECT_KIND_FOREIGN: {
			SetOutputValues(params.GetPinBuffer(NSN_Input));
			break;
		}
		case NOS_OBJECT_KIND_ARRAY: {
			for (size_t i = 0; i < ArraySize; ++i)
			{
				auto pinId = GetPinId(nos::Name("Output " + std::to_string(i)));
				if (!pinId)
					continue;
				ObjectRef elementRef;
				if (NOS_RESULT_SUCCESS !=
					nosEngine.ObjectAPI->GetArrayElement(params.GetPinObject(NSN_Input), i, &elementRef.GetStorage()))
				{
					nosEngine.LogE("Failed to get array element %d", i);
					continue;
				}
				SetPinObject(*pinId, elementRef);
			}
			break;
		}
		case NOS_OBJECT_KIND_COMPOSITE: {
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

} // namespace nos::reflect
