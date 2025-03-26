// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

// Includes
#include <Nodos/PluginHelpers.hpp>
#include <glm/glm.hpp>
#include <Builtins_generated.h>
#include <Module_generated.h>

#include <nosVulkanSubsystem/nosVulkanSubsystem.h>

NOS_INIT_WITH_MIN_REQUIRED_MINOR(4)
NOS_VULKAN_INIT()

NOS_BEGIN_IMPORT_DEPS()
	NOS_VULKAN_IMPORT()
NOS_END_IMPORT_DEPS()

NOS_REGISTER_NAME(Input);
NOS_REGISTER_NAME(Output);
NOS_REGISTER_NAME(In);
NOS_REGISTER_NAME(Out);
NOS_REGISTER_NAME(Path);
NOS_REGISTER_NAME(sRGB);

namespace nos::utilities
{

enum Utilities : int
{	// CPU nodes
	Resize = 0,
	ChannelViewer,
	Merge,
	Time,
	StbiLoad,
	WriteImage,
	CPUSleep,
	UploadBuffer,
	Buffer2Texture,
	Texture2Buffer,
	ShowStatusNode,
	Sink,
	PropagateExecution,
	UploadBufferProvider,
	BoundedQueue,
	RingBuffer,
	Host,
	DeinterlacedBoundedTextureQueue,
	DeinterlacedBufferRing,
	SyncMultiOutlet,
	ConditionalTrigger,
	TriggerOnAnyInput,
	SwitchTrigger,
	PrintLog,
	ScheduleOnRequest,
	Count
};

nosResult RegisterMerge(nosNodeFunctions*);
nosResult RegisterTime(nosNodeFunctions*);
nosResult RegisterStbiLoad(nosNodeFunctions*);
nosResult RegisterWriteImage(nosNodeFunctions*);
nosResult RegisterChannelViewer(nosNodeFunctions*);
nosResult RegisterResize(nosNodeFunctions*);
nosResult RegisterCPUSleep(nosNodeFunctions*);
nosResult RegisterUploadBuffer(nosNodeFunctions*);
nosResult RegisterBuffer2Texture(nosNodeFunctions*);
nosResult RegisterTexture2Buffer(nosNodeFunctions*);
nosResult RegisterShowStatusNode(nosNodeFunctions*);
nosResult RegisterSink(nosNodeFunctions*);
nosResult RegisterPropagateExecution(nosNodeFunctions*);
nosResult RegisterUploadBufferProvider(nosNodeFunctions*);
nosResult RegisterBoundedQueue(nosNodeFunctions*);
nosResult RegisterRingBuffer(nosNodeFunctions*);
nosResult RegisterHost(nosNodeFunctions*);
nosResult RegisterPin2Json(nosNodeFunctions*);
nosResult RegisterJson2Pin(nosNodeFunctions*);
nosResult RegisterDeinterlacedBoundedTextureQueue(nosNodeFunctions*);
nosResult RegisterDeinterlacedBufferRing(nosNodeFunctions*);
nosResult RegisterSyncMultiOutlet(nosNodeFunctions*);
nosResult RegisterConditionalTrigger(nosNodeFunctions*);
nosResult RegisterTriggerOnAnyInput(nosNodeFunctions*);
nosResult RegisterSwitchTrigger(nosNodeFunctions*);
nosResult RegisterPrintLog(nosNodeFunctions*);
nosResult RegisterScheduleOnRequest(nosNodeFunctions*);

static nosResult MigrateReadImageToGraph(nosFbNodePtr node, nosBuffer* outBuffer);
nosResult NOSAPI_CALL ExportNodeFunctions(size_t* outSize, nosNodeFunctions** outList)
{
	*outSize = Utilities::Count + 1;
	if (!outList)
		return NOS_RESULT_SUCCESS;

#define GEN_CASE_NODE(name)					\
	case Utilities::name: {					\
		auto ret = Register##name(node);	\
		if (NOS_RESULT_SUCCESS != ret)		\
			return ret;						\
		break;								\
	}

	for (int i = 0; i < Utilities::Count; ++i)
	{
		auto node = outList[i];
		switch ((Utilities)i) {
		default:
			break;
			GEN_CASE_NODE(Merge)
			GEN_CASE_NODE(Time)
			GEN_CASE_NODE(StbiLoad)
			GEN_CASE_NODE(WriteImage)
			GEN_CASE_NODE(ChannelViewer)
			GEN_CASE_NODE(Resize)
			GEN_CASE_NODE(CPUSleep)
			GEN_CASE_NODE(UploadBuffer)
			GEN_CASE_NODE(Buffer2Texture)
			GEN_CASE_NODE(Texture2Buffer)
			GEN_CASE_NODE(ShowStatusNode)
			GEN_CASE_NODE(Sink)
			GEN_CASE_NODE(PropagateExecution)
			GEN_CASE_NODE(UploadBufferProvider)
			GEN_CASE_NODE(BoundedQueue)
			GEN_CASE_NODE(RingBuffer)
			GEN_CASE_NODE(Host)
			GEN_CASE_NODE(DeinterlacedBoundedTextureQueue)
			GEN_CASE_NODE(DeinterlacedBufferRing)
			GEN_CASE_NODE(SyncMultiOutlet)
			GEN_CASE_NODE(ConditionalTrigger)
			GEN_CASE_NODE(TriggerOnAnyInput)
			GEN_CASE_NODE(SwitchTrigger)
			GEN_CASE_NODE(PrintLog)
			GEN_CASE_NODE(ScheduleOnRequest)
		}
	}
	
	*outList[(int)Utilities::Count] = nosNodeFunctions{
		.ClassName = NOS_NAME("nos.utilities.ReadImage"),
		.MigrateNode = MigrateReadImageToGraph
	};
	return NOS_RESULT_SUCCESS;
}

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

static nosResult MigrateReadImageToGraph(nosFbNodePtr node, nosBuffer* outBuffer) {
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
					if(copyData)
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

extern "C"
{
NOSAPI_ATTR nosResult NOSAPI_CALL nosExportPlugin(nosPluginFunctions* out)
{
	out->ExportNodeFunctions = ExportNodeFunctions;
	out->GetRenamedTypes = [](nosName* outRenamedFrom, nosName* outRenamedTo, size_t* outSize) {
		if (!outRenamedFrom)
		{
			*outSize = 8;
			return;
		}
		// clang-format off
		outRenamedFrom[0] = NOS_NAME("nos.fb.ChannelViewerChannels"); outRenamedTo[0] = NOS_NAME("nos.utilities.ChannelViewerChannels");
		outRenamedFrom[1] = NOS_NAME("nos.fb.ChannelViewerFormats"); outRenamedTo[1] = NOS_NAME("nos.utilities.ChannelViewerFormats");
		outRenamedFrom[2] = NOS_NAME("nos.fb.GradientKind"); outRenamedTo[2] = NOS_NAME("nos.utilities.GradientKind");
		outRenamedFrom[3] = NOS_NAME("nos.fb.BlendMode"); outRenamedTo[3] = NOS_NAME("nos.utilities.BlendMode");
		outRenamedFrom[4] = NOS_NAME("nos.fb.ResizeMethod"); outRenamedTo[4] = NOS_NAME("nos.utilities.ResizeMethod");
		outRenamedFrom[5] = NOS_NAME("nos.fb.Source"); outRenamedTo[5] = NOS_NAME("nos.utilities.Source");
		outRenamedFrom[6] = NOS_NAME("nos.fb.Channel"); outRenamedTo[6] = NOS_NAME("nos.utilities.Channel");
		outRenamedFrom[7] = NOS_NAME("nos.plugin.switcher.TextureSwitcherChannel"); outRenamedTo[7] = NOS_NAME("nos.utilities.TextureSwitcherChannel");
		// clang-format on
	};
	return NOS_RESULT_SUCCESS;
}
}
}	
