// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/PluginHelpers.hpp>

namespace nos::math
{
void RegisterSineWave(nosNodeFunctions* node)
{
	node->ClassName = NOS_NAME("nos.math.SineWave");
	node->MigrateNode = [](nosFbNodePtr node, nosBuffer* outBuffer) {
		// TODO: Remove these when automatic migration is sufficiently comprehensive in Nodos
		auto pluginVersion = node->plugin_version();
		bool needsMigration = !pluginVersion || pluginVersion->major() <= 1 && pluginVersion->minor() < 22;
		if (!needsMigration)
			return NOS_RESULT_SUCCESS;
		fb::TNode oldNode;
		node->UnPackTo(&oldNode);
		
		nosBuffer newNodeDefinitionBuffer;
		nosEngine.GetAssetAsType(nosEngine.Plugin->Id,
			"Config/SineWave.nosdef",
			NOS_NAME("nos.fb.NodeDefinitions"),
			&newNodeDefinitionBuffer);

		auto newNodeDefs = flatbuffers::GetRoot<fb::NodeDefinitions>(newNodeDefinitionBuffer.Data);
		// Get the first node definition
		auto nodeDef = newNodeDefs->nodes()->Get(0);
		auto newNodeFb = nodeDef->node();
		fb::TNode newNode;
		newNodeFb->UnPackTo(&newNode);

		// Migrate values from the old node to the new node and update source pins
		{
			std::unordered_map<std::string, fb::TPin*> newPinMap;
			for (auto& newPin : newNode.pins) {
				newPinMap[newPin->name] = newPin.get();
			}
			std::unordered_map<uuid, fb::TPin*> innerPinMap;
			if (auto* graph = newNode.contents.AsGraph())
			{
				for (auto& node : graph->nodes)
					for (auto& innerNodePin : node->pins)
						innerPinMap[innerNodePin->id] = innerNodePin.get();
			}
			for (auto& pin : oldNode.pins)
			{
				auto it = newPinMap.find(pin->name);
				if (it == newPinMap.end())
					continue;
				fb::TPin* newPin = it->second;
				newPin->id = pin->id;
				nos::Buffer oldPinData = pin->data;
				newPin->data = nos::Buffer::From(static_cast<double>(*oldPinData.As<float>()));
				if (auto* portal = newPin->contents.AsPortalPin())
				{
					auto innerIt = innerPinMap.find(portal->source_id);
					if (innerIt != innerPinMap.end())
						innerIt->second->data = newPin->data;
				}
			}
		}

		auto nodeBuffer = nos::EngineBuffer::CopyFrom(newNode);
		*outBuffer = nodeBuffer.Release();
		return NOS_RESULT_SUCCESS;
	};
}
} // namespace nos::math

