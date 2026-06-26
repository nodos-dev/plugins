// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include "TypeCommon.h"

#include <unordered_map>
#include <unordered_set>

namespace nos::reflect
{
// Make Dict: bundles a set of user-named input pins into a nos.Dict object. Each input pin is a
// field - the pin's display name is the dict key, its connected object is the value. Fields are
// added/removed from the node's context menu (à la nos.math.Eval); a field's value type can be set or
// reset to generic from the field pin's menu. Otherwise a field resolves its type, independently, on
// connection.
struct MakeDictNode : NodeContext
{
	enum MenuCommand : uint32_t
	{
		ADD_FIELD = 1,
		REMOVE_FIELD = 2,
		RESET_TYPE = 3,
		SET_TYPE_BASE = 0x10000, // command = SET_TYPE_BASE + index into AvailableTypes
	};

	std::vector<nos::Name> AvailableTypes;

	nosResult OnCreate(nosFbNodePtr node) override
	{
		// Field pins restored from a saved graph can come back orphaned/passive; reactivate them so they
		// are live again on load (à la nos.math.Eval).
		std::vector<uuid> toUnorphan;
		for (auto* pin : *node->pins())
		{
			if (pin->show_as() != fb::ShowAs::INPUT_PIN)
				continue;
			if (auto orphan = pin->orphan_state())
				if (orphan->type() == fb::PinOrphanStateType::PASSIVE ||
					orphan->type() == fb::PinOrphanStateType::ORPHAN)
					toUnorphan.push_back(*pin->id());
		}
		for (auto const& id : toUnorphan)
			SetPinOrphanState(id, fb::PinOrphanStateType::ACTIVE);
		return NOS_RESULT_SUCCESS;
	}

	nosResult OnResolvePinDataTypes(nosResolvePinDataTypesParams* params) override
	{
		// The engine pre-fills every still-generic pin's resolved type with the incoming type. Only the
		// instigated pin should adopt it; reset the others to their own (generic) type so each field
		// resolves independently rather than all fields snapping to the first connected type.
		for (size_t i = 0; i < params->PinCount; i++)
		{
			auto& pin = params->Pins[i];
			pin.OutResolvedTypeName = (nos::Name(pin.Name) == nos::Name(params->InstigatorPinName))
										  ? params->IncomingTypeName
										  : pin.TypeName;
		}
		return NOS_RESULT_SUCCESS;
	}

	size_t NextFieldIndex()
	{
		size_t i = 0;
		while (GetPin(nos::Name("Field " + std::to_string(i))))
			i++;
		return i;
	}

	void AddField()
	{
		auto name = nos::Name("Field " + std::to_string(NextFieldIndex()));
		AddPin(PinBuilder(name)
				   .SetDisplayName(name)
				   .SetTypeName(NSN_TypeNameGeneric)
				   .SetShowAs(nos::fb::ShowAs::INPUT_PIN)
				   .SetCanShowAs(nos::fb::CanShowAs::INPUT_PIN_OR_PROPERTY));
	}

	// Set or reset a field pin's declared type (used from the field pin's context menu).
	void SetPinType(uuid const& pinId, nos::Name typeName)
	{
		flatbuffers::FlatBufferBuilder fbb;
		std::vector<flatbuffers::Offset<PartialPinUpdate>> updates;
		updates.push_back(CreatePartialPinUpdateDirect(fbb, &pinId, 0, 0, nos::Name(typeName).AsCStr()));
		HandleEvent(CreateAppEvent(
			fbb, CreatePartialNodeUpdateDirect(fbb, &NodeId, ClearFlags::NONE, 0, 0, 0, 0, 0, 0, 0, &updates)));
	}

	std::vector<nos::Name> const& GetAvailableTypes()
	{
		if (!AvailableTypes.empty())
			return AvailableTypes;
		size_t count = 0;
		if (nosEngine.GetPinDataTypeNames(nullptr, &count) == NOS_RESULT_FAILED)
			return AvailableTypes;
		std::vector<nosName> names(count);
		nosEngine.GetPinDataTypeNames(names.data(), &count);
		for (auto n : names)
			if (n != NSN_TypeNameGeneric)
				AvailableTypes.push_back(n);
		std::sort(AvailableTypes.begin(), AvailableTypes.end(),
				  [](nos::Name a, nos::Name b) { return a.AsString() < b.AsString(); });
		return AvailableTypes;
	}

	void OnNodeMenuRequested(nosContextMenuRequestPtr request) override
	{
		SendContextMenu(request, [](ContextMenuBuilder& menu) { menu.Item("Add Field", ADD_FIELD); });
	}

	void OnPinMenuRequested(nos::Name pinName, nosContextMenuRequestPtr request) override
	{
		if (pinName == NSN_Output)
			return;
		auto const& types = GetAvailableTypes();
		SendContextMenu(request, [&](ContextMenuBuilder& menu) {
			menu.Submenu("Set Type", [&](ContextMenuBuilder& sub) {
				for (size_t i = 0; i < types.size(); i++)
					sub.Item(types[i].AsString(), uint32_t(SET_TYPE_BASE + i));
			});
			menu.Item("Reset Type", RESET_TYPE);
			menu.Item("Remove Field", REMOVE_FIELD);
		});
	}

	void OnMenuCommand(uuid const& itemId, uint32_t cmd) override
	{
		switch (cmd)
		{
		case ADD_FIELD:
			AddField();
			return;
		case REMOVE_FIELD:
			if (itemId != NodeId)
				DeletePin(itemId);
			return;
		case RESET_TYPE:
			if (itemId != NodeId)
				SetPinType(itemId, NSN_TypeNameGeneric);
			return;
		default:
			if (cmd >= SET_TYPE_BASE && itemId != NodeId)
			{
				size_t idx = cmd - SET_TYPE_BASE;
				auto const& types = GetAvailableTypes();
				if (idx < types.size())
					SetPinType(itemId, types[idx]);
			}
			return;
		}
	}

	std::vector<std::pair<nos::Name, uint64_t>> LastSignature; // sorted (key, value object id) last published

	nosResult ExecuteNode(NodeExecuteParams const& params) override
	{
		std::vector<CompositeObjectField> fields;
		std::vector<std::pair<nos::Name, uint64_t>> signature;
		std::unordered_set<nos::Name> seenKeys;
		for (auto const& [name, info] : params)
		{
			if (name == NSN_Output)
				continue;
			auto* pin = GetPin(name);
			if (!pin || pin->ShowAs != fb::ShowAs::INPUT_PIN)
				continue;
			ObjectRef value = params.GetPinObject(name);
			if (!value.IsValid())
				continue;
			if (!seenKeys.insert(pin->DisplayName).second)
			{
				nosEngine.LogW("Make Dict: duplicate key '%s' ignored", pin->DisplayName.AsString().c_str());
				continue;
			}
			signature.push_back({pin->DisplayName, value.GetObjectId().Value});
			fields.push_back({pin->DisplayName, std::move(value)});
		}
		// Only build & publish a new Dict object when the inputs actually changed - don't churn a fresh
		// object (and re-trigger downstream) on every execute.
		std::sort(signature.begin(), signature.end(),
				  [](auto const& a, auto const& b) { return a.first.AsString() < b.first.AsString(); });
		if (signature == LastSignature)
			return NOS_RESULT_SUCCESS;
		LastSignature = std::move(signature);
		auto dict = DictRef::Create(fields);
		if (!dict)
			return NOS_RESULT_FAILED;
		SetPinObject(NSN_Output, *dict);
		params.MarkAllOutsDirty = false;
		return NOS_RESULT_SUCCESS;
	}
};

// Break Dict: decomposes a nos.Dict into one output pin per entry. Output pins are inferred from the
// incoming dict's actual entries (keys + per-entry concrete type), both the moment it is connected
// (OnPinConnected, before the node is ever scheduled) and when its value changes while running
// (OnPinObjectChanged). Managed output pins are tracked locally (OutputPins) rather than read back from
// the pin map, because pin create/delete events are processed asynchronously - so RebuildOutputs stays
// idempotent even when it fires several times in one tick.
struct BreakDictNode : NodeContext
{
	struct Output
	{
		uuid Id;
		nos::Name Type;
	};
	std::unordered_map<nos::Name, Output> OutputPins; // dict key -> managed output pin

	nosResult OnCreate(nosFbNodePtr node) override
	{
		// Restore managed outputs from the pins persisted in the saved graph, reactivating any that came
		// back orphaned/passive (à la nos.math.Eval).
		std::vector<uuid> toUnorphan;
		for (auto* pin : *node->pins())
		{
			nos::Name name(pin->name()->string_view());
			if (pin->show_as() == fb::ShowAs::OUTPUT_PIN && name != NSN_Input)
			{
				OutputPins[name] = {*pin->id(), nos::Name(pin->type_name()->string_view())};
				if (auto orphan = pin->orphan_state())
					if (orphan->type() == fb::PinOrphanStateType::PASSIVE ||
						orphan->type() == fb::PinOrphanStateType::ORPHAN)
						toUnorphan.push_back(*pin->id());
			}
		}
		for (auto const& id : toUnorphan)
			SetPinOrphanState(id, fb::PinOrphanStateType::ACTIVE);
		return NOS_RESULT_SUCCESS;
	}

	void OnPinObjectChanged(nos::Name pinName, uuid const& pinId, nosObjectId newHandle) override
	{
		if (pinName == NSN_Input && newHandle.Value)
			RebuildOutputs(DictRef::FromObjectId(newHandle));
	}

	// Also infer outputs the moment a Dict is connected: the node may not be scheduled yet, so
	// OnPinObjectChanged would not fire. If the engine already has the connected pin's object, rebuild
	// from it directly; otherwise the upstream hasn't produced it yet, so request a schedule to pull the
	// value through - OnPinObjectChanged then fires with the resolved object and rebuilds the outputs.
	void OnPinConnected(nos::Name pinName, uuid const& connectedPin, nosObjectId connectedObject) override
	{
		if (pinName != NSN_Input)
			return;
		if (connectedObject)
			RebuildOutputs(DictRef::FromObjectId(connectedObject));
		else
			SendScheduleRequest(1);
	}

	void RebuildOutputs(DictRef const& dict)
	{
		if (!dict.IsValid())
			return;
		std::unordered_set<nos::Name> desired;
		for (auto& field : dict)
		{
			desired.insert(field.Name);
			nos::Name type = field.Object.GetTypeName();
			auto it = OutputPins.find(field.Name);
			if (it != OutputPins.end())
			{
				if (it->second.Type == type)
					continue; // already have an output pin of the right type
				DeletePin(it->second.Id); // value type changed: recreate with the new type
				OutputPins.erase(it);
			}
			uuid id = nosEngine.GenerateID();
			AddPin(PinBuilder(field.Name)
					   .SetId(id)
					   .SetDisplayName(field.Name)
					   .SetTypeName(type)
					   .SetShowAs(nos::fb::ShowAs::OUTPUT_PIN)
					   .SetCanShowAs(nos::fb::CanShowAs::OUTPUT_PIN_OR_PROPERTY));
			OutputPins[field.Name] = {id, type};
		}
		for (auto it = OutputPins.begin(); it != OutputPins.end();)
		{
			if (desired.contains(it->first))
				++it;
			else
			{
				DeletePin(it->second.Id);
				it = OutputPins.erase(it);
			}
		}
	}

	std::unordered_map<nos::Name, uint64_t> LastServed; // output key -> last object id set on it

	nosResult ExecuteNode(NodeExecuteParams const& params) override
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
			auto it = LastServed.find(field.Name);
			if (it != LastServed.end() && it->second == id)
				continue;
			SetPinObject(pin->Id, field.Object);
			LastServed[field.Name] = id;
		}
		params.MarkAllOutsDirty = false;
		return NOS_RESULT_SUCCESS;
	}
};

nosResult RegisterMakeDict(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NSN_MakeDict, MakeDictNode, fn);
	return NOS_RESULT_SUCCESS;
}

nosResult RegisterBreakDict(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NSN_BreakDict, BreakDictNode, fn);
	return NOS_RESULT_SUCCESS;
}

} // namespace nos::reflect
