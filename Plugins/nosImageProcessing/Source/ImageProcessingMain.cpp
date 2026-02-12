// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>
#include <glm/glm.hpp>
#include <Builtins_generated.h>
#include <Module_generated.h>

#include <nosSysVulkan/nosVulkanSubsystem.h>

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

namespace nos::imageprocessing
{

enum ImageProcessing : int
{
	AutoResize = 0,
	BoxFit,
	CalculateDispatchSize,
	ChannelViewer,
	Checkerboard,
	Color,
	Gradient,
	StbiLoad,
	LoadCubeLUT,
	MeanSquaredError,
	Offset,
	PSNR,
	QuadMerge,
	ReadImage,
	ReduceTexture,
	SevenSegment,
	SplitWipe,
	Swizzle,
	TextureSwitcher,
	YADIF,
	YADIFWithAutoDispatchSize,
	WriteImage,
	Count
};

// Forward declarations
nosResult RegisterBoxFit(nosNodeFunctions*);
nosResult RegisterChannelViewer(nosNodeFunctions*);
nosResult RegisterLoadCubeLUT(nosNodeFunctions*);
nosResult RegisterReduceTexture(nosNodeFunctions*);
nosResult RegisterStbiLoad(nosNodeFunctions*);
nosResult RegisterWriteImage(nosNodeFunctions*);

nosResult NOSAPI_CALL ExportNodeFunctions(size_t* outSize, nosNodeFunctions** outList)
{
	*outSize = ImageProcessing::Count;
	if (!outList)
		return NOS_RESULT_SUCCESS;

#define GEN_CASE_NODE(name)					\
	case ImageProcessing::name: {			\
		auto ret = Register##name(node);	\
		if (NOS_RESULT_SUCCESS != ret)		\
			return ret;						\
		break;								\
	}

	for (int i = 0; i < ImageProcessing::Count; ++i)
	{
		auto node = outList[i];
		switch ((ImageProcessing)i) {
		default:
			break;
			GEN_CASE_NODE(BoxFit)
			GEN_CASE_NODE(ChannelViewer)
			GEN_CASE_NODE(LoadCubeLUT)
			GEN_CASE_NODE(ReduceTexture)
			GEN_CASE_NODE(StbiLoad)
			GEN_CASE_NODE(WriteImage)
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
			*outSize = 6;
			return;
		}
		// Migrated from nos.utilities to nos.imageprocessing
		outRenamedFrom[0] = NOS_NAME("nos.utilities.ChannelViewerChannels"); outRenamedTo[0] = NOS_NAME("nos.imageprocessing.ChannelViewerChannels");
		outRenamedFrom[1] = NOS_NAME("nos.utilities.ChannelViewerFormats"); outRenamedTo[1] = NOS_NAME("nos.imageprocessing.ChannelViewerFormats");
		outRenamedFrom[2] = NOS_NAME("nos.utilities.GradientKind"); outRenamedTo[2] = NOS_NAME("nos.imageprocessing.GradientKind");
		outRenamedFrom[3] = NOS_NAME("nos.utilities.Source"); outRenamedTo[3] = NOS_NAME("nos.imageprocessing.Source");
		outRenamedFrom[4] = NOS_NAME("nos.utilities.Channel"); outRenamedTo[4] = NOS_NAME("nos.imageprocessing.Channel");
		outRenamedFrom[5] = NOS_NAME("nos.plugin.switcher.TextureSwitcherChannel"); outRenamedTo[5] = NOS_NAME("nos.imageprocessing.TextureSwitcherChannel");
	};
	return NOS_RESULT_SUCCESS;
}
}
}
