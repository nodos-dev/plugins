// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

// Includes
#include <Nodos/Plugin.hpp>
#include <glm/glm.hpp>
#include <Builtins_generated.h>
#include <Module_generated.h>

#include <nosVulkanSubsystem/nosVulkanSubsystem.h>

NOS_INIT()
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
	LayoutDrawer,
	FreeLayout,
	GridLayout,
	FreeOutputLayout,
	GridOutputLayout,
	ScheduleOnRequest,
	ReadImage,
	BoxFit,
	ReduceTexture,
	MultiLiveOut,
	LoadCubeLUT,
	RepeatingJunction,
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
nosResult RegisterReadImage(nosNodeFunctions*);
nosResult RegisterBoxFit(nosNodeFunctions*);
nosResult RegisterLayoutDrawer(nosNodeFunctions*);
nosResult RegisterFreeLayout(nosNodeFunctions*);
nosResult RegisterGridLayout(nosNodeFunctions*);
nosResult RegisterFreeOutputLayout(nosNodeFunctions*);
nosResult RegisterGridOutputLayout(nosNodeFunctions*);
nosResult RegisterReduceTexture(nosNodeFunctions*);
nosResult RegisterMultiLiveOut(nosNodeFunctions*);
nosResult RegisterLoadCubeLUT(nosNodeFunctions*);
nosResult RegisterRepeatingJunction(nosNodeFunctions*);

nosResult NOSAPI_CALL ExportNodeFunctions(size_t* outSize, nosNodeFunctions** outList)
{
	*outSize = Utilities::Count;
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
			GEN_CASE_NODE(ReadImage)
			GEN_CASE_NODE(BoxFit)
			GEN_CASE_NODE(LayoutDrawer)
			GEN_CASE_NODE(FreeLayout)
			GEN_CASE_NODE(GridLayout)
			GEN_CASE_NODE(FreeOutputLayout)
			GEN_CASE_NODE(GridOutputLayout)
			GEN_CASE_NODE(ReduceTexture)
			GEN_CASE_NODE(MultiLiveOut)
			GEN_CASE_NODE(LoadCubeLUT)
			GEN_CASE_NODE(RepeatingJunction)
		}
	}
	
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
