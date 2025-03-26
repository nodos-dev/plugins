// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

// Includes
#include <Nodos/PluginHelpers.hpp>
#include <glm/glm.hpp>
#include <Builtins_generated.h>
#include <Module_generated.h>

namespace nos::utilities
{

static nos::fb::TPin* UpdateEveryReferredBy(nos::fb::TGraph* graph, nos::fb::UUID oldId, nos::fb::UUID newId) {
	for (auto const& node : graph->nodes)
		for (auto const& pin : node->pins)
			for (auto& referredById : pin->referred_by)
				if (referredById == oldId) {
					referredById = newId;
				}
	return nullptr;
};

static nosResult UpdateSourcePinData(nos::fb::TGraph* graph, nos::fb::UUID pinId, std::vector<uint8_t> const& data) {
	for (auto const& node : graph->nodes)
		for (auto const& pin : node->pins)
			if (pin->id == pinId) {
				pin->data = data;
				return NOS_RESULT_SUCCESS;
			}
	return NOS_RESULT_FAILED;
}

nosResult MigrateReadImageToGraph(nosFbNodePtr node, nosBuffer* outBuffer) {
	auto pluginVersion = node->plugin_version();
	bool needsMigration = !pluginVersion || pluginVersion->major() <= 3 && pluginVersion->minor() < 10;
	if (!needsMigration)
		return NOS_RESULT_SUCCESS;
	fb::TNode cur;
	node->UnPackTo(&cur);
	char path[256];
	nosEngine.GetModuleFolderPath(nosEngine.Module->Id, 256, path);
	std::string ReadImageGraphFile = ReadToString(std::string(path) + "/Config/ReadImage.nosdef");
	auto graphBuffer = GenerateBufferFromJson(NOS_NAME(nos::fb::NodeDefinitions::GetFullyQualifiedName()), ReadImageGraphFile.c_str());
	if (!graphBuffer) {
		// Failed to read graph file
		nosEngine.LogE("Failed to read graph file");
		return NOS_RESULT_FAILED;
	}
	nos::fb::TNodeDefinitions read;
	graphBuffer->As<nos::fb::NodeDefinitions>()->UnPackTo(&read);
	nos::fb::TNode outNode;
	outNode = *read.nodes[0]->node;
	auto matchPinIds = [&](const char* fromPinName, const char* toPinName, bool copyData, const char* sourceFuncName = nullptr) {
		auto matchWithSourcePin = [&](nos::fb::TPin* targetPin) -> bool {
			auto copyFromSourceToTargetPin = [&](nos::fb::TPin* sourcePin) {
				auto prevId = targetPin->id;
				targetPin->id = sourcePin->id;
				targetPin->show_as = sourcePin->show_as;

				if (auto targetPinPortal = targetPin->contents.AsPortalPin()) {
					UpdateEveryReferredBy(outNode.contents.AsGraph(), prevId, targetPin->id);
					if (copyData)
						UpdateSourcePinData(outNode.contents.AsGraph(), targetPinPortal->source_id, sourcePin->data);
				}
				else if (copyData && targetPin->type_name == sourcePin->type_name)
					targetPin->data = sourcePin->data;
				};

			for (auto const& sourcePin : cur.pins)
				if (sourcePin->name == fromPinName) {
					copyFromSourceToTargetPin(sourcePin.get());
					return true;
				}
			for (auto const& sourceFunc : cur.functions) {
				if (sourceFuncName && sourceFunc->name == sourceFuncName)
					for (auto const& sourceFuncPin : sourceFunc->pins)
						if (sourceFuncPin->name == fromPinName) {
							copyFromSourceToTargetPin(sourceFuncPin.get());
							return true;
						}
			}
			return false;
			};
		bool isFound = false;
		for (auto& targetPin : outNode.pins) {
			if (isFound)
				break;
			if (targetPin->name == toPinName) {
				isFound = matchWithSourcePin(targetPin.get());
			}
		}

		if (!isFound) {
			for (auto& targetFunc : outNode.functions)
				for (auto& targetFuncPin : targetFunc->pins)
					if (targetFuncPin->name == toPinName) {
						isFound = matchWithSourcePin(targetFuncPin.get());
						break;
					}
		}
		assert(isFound);
		};
	matchPinIds("Path", "Path", true);
	matchPinIds("sRGB", "sRGB", true);
	matchPinIds("Out", "Out", false);
	matchPinIds("OutExe", "OnLoaded", false, "OnImageLoaded");
	matchPinIds("InExe", "Load", false, "ReadImage_Load");
	auto nodeBuffer = EngineBuffer::CopyFrom(outNode);
	*outBuffer = nodeBuffer.Release();
	return NOS_RESULT_SUCCESS;
}
}