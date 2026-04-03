// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/PluginHelpers.hpp>

namespace nos::utilities
{

struct MultiLiveOutNode : NodeContext
{
	MultiLiveOutNode(nosFbNodePtr node) : NodeContext(node)
	{
		for (auto* pin : *node->pins())
		{
			SetPinOrphanState(*pin->id(), nos::fb::PinOrphanStateType::ACTIVE);
			auto index = GetPinIndex(pin->name()->string_view());
			if (!index)
			{
				nosEngine.LogE("Failed to parse index from pin name: %s", pin->name()->c_str());
				continue;
			}
			if (pin->show_as() == nosFbShowAs::OUTPUT_PIN)
				IndexToPairs[*index].second = uuid(*pin->id());
			else
				IndexToPairs[*index].first = uuid(*pin->id());
		}
	}

	void OnNodeUpdated(nosNodeUpdate const* update) override
	{
		if (update->Type == NOS_NODE_UPDATE_PIN_CREATED)
		{
			auto* pin = update->PinCreated;
			auto index = GetPinIndex(pin->name()->string_view());
			if (!index)
				return;
			if (pin->show_as() == nosFbShowAs::OUTPUT_PIN)
				IndexToPairs[*index].second = uuid(*pin->id());
			else
				IndexToPairs[*index].first = uuid(*pin->id());
		}
		else if (update->Type == NOS_NODE_UPDATE_PIN_DELETED)
		{
			for (auto it = IndexToPairs.begin(); it != IndexToPairs.end(); ++it)
			{
				if (it->second.first == update->PinDeleted || it->second.second == update->PinDeleted)
				{
					IndexToPairs.erase(it);
					break;
				}
			}
		}
	}

	void OnMenuRequested(nosContextMenuRequestPtr request) override
	{
		flatbuffers::FlatBufferBuilder fbb;
		std::vector<flatbuffers::Offset<nos::ContextMenuItem>> items;
		if (*request->item_id() == NodeId)
			items.push_back(nos::CreateContextMenuItemDirect(fbb, "Add New Pair", 1));
		else
		{
			auto* pin = GetPin(*request->item_id());
			if (!pin)
				return;
			if (pin->Name == NOS_NAME("Input_0") || pin->Name == NOS_NAME("Output_0"))
				return;
			items.push_back(nos::CreateContextMenuItemDirect(fbb, "Remove Pair", 1));
		}
		HandleEvent(CreateAppEvent(
			fbb, CreateAppContextMenuUpdate(
				fbb, request->item_id(), request->pos(), request->instigator(), fbb.CreateVector(items))));
	}

	void OnMenuCommand(uuid const& itemID, uint32_t cmd) override
	{
		flatbuffers::FlatBufferBuilder fbb;
		if (itemID == NodeId)
		{
			int index = 0;
			for (; index < (int)IndexToPairs.size(); index++)
			{
				if (!IndexToPairs.contains(index))
					break;
			}
			fb::TPin outPin;
			outPin.id = uuid(nosEngine.GenerateID());
			outPin.name = "Output_" + std::to_string(index);
			outPin.type_name = NOS_NAME("nos.Generic");
			outPin.live = true;
			outPin.show_as = fb::ShowAs::OUTPUT_PIN;
			outPin.can_show_as = fb::CanShowAs::OUTPUT_PIN_ONLY;

			fb::TPin inPin;
			inPin.id = uuid(nosEngine.GenerateID());
			inPin.name = "Input_" + std::to_string(index);
			inPin.type_name = NOS_NAME("nos.Generic");
			inPin.show_as = fb::ShowAs::INPUT_PIN;
			inPin.can_show_as = fb::CanShowAs::INPUT_PIN_ONLY;

			nos::TPartialNodeUpdate update;
			update.node_id = NodeId;
			update.pins_to_add.emplace_back(std::make_unique<fb::TPin>(std::move(outPin)));
			update.pins_to_add.emplace_back(std::make_unique<fb::TPin>(std::move(inPin)));
			HandleEvent(CreateAppEvent(fbb, nos::CreatePartialNodeUpdate(fbb, &update)));
			IndexToPairs[index] = {uuid(inPin.id), uuid(outPin.id)};
		}
		else
		{
			auto* pin = GetPin(itemID);
			if (!pin)
				return;
			auto index = GetPinIndex(pin->Name.AsString());
			if (!index)
			{
				nosEngine.LogE("Failed to parse index from pin name: %s", pin->Name.AsCStr());
				return;
			}
			nos::TPartialNodeUpdate update;
			update.node_id = NodeId;
			update.pins_to_delete = {IndexToPairs[*index].first, IndexToPairs[*index].second};
			HandleEvent(CreateAppEvent(fbb, nos::CreatePartialNodeUpdate(fbb, &update)));
			IndexToPairs.erase(*index);
		}
	}

	nosResult OnResolvePinDataTypes(nosResolvePinDataTypesParams* params) override
	{
		auto pinName = nos::Name(params->InstigatorPinName).AsString();
		auto index = GetPinIndex(pinName);
		if (!index.has_value())
		{
			strcpy(params->OutErrorMessage, "Failed to parse pin index from pin name.");
			return NOS_RESULT_FAILED;
		}
		auto const& [firstId, secondId] = IndexToPairs[*index];
		for (size_t i = 0; i < params->PinCount; i++)
		{
			auto& pin = params->Pins[i];
			if (pin.Id == firstId || pin.Id == secondId)
				pin.OutResolvedTypeName = params->IncomingTypeName;
			else
				pin.OutResolvedTypeName = NOS_NAME("nos.Generic");
		}
		return NOS_RESULT_SUCCESS;
	}

	std::optional<int32_t> GetPinIndex(std::string_view pinName) const
	{
		auto indexPos = pinName.find_last_of('_');
		if (indexPos == std::string::npos)
			return std::nullopt;
		try
		{
			return std::stoi(std::string(pinName.substr(indexPos + 1)));
		}
		catch (...)
		{
			nosEngine.LogE("Failed to parse index from pin name: %s", std::string(pinName).c_str());
			return std::nullopt;
		}
	}

	nosResult ExecuteNode(nosNodeExecuteParams* params) override
	{
		for (auto const& [_, idPair] : IndexToPairs)
		{
			for (size_t i = 0; i < params->PinCount; ++i)
			{
				auto& pin = params->Pins[i];
				if (pin.Id == idPair.first && pin.Data)
				{
					nosEngine.SetPinValue(idPair.second, *pin.Data);
					break;
				}
			}
		}
		return NOS_RESULT_SUCCESS;
	}

	std::unordered_map<int32_t, std::pair<nos::uuid, nos::uuid>> IndexToPairs;
};

nosResult RegisterMultiLiveOut(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("MultiLiveOut"), MultiLiveOutNode, fn)
	return NOS_RESULT_SUCCESS;
}

} // namespace nos::utilities
