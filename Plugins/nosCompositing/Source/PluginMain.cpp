// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>
#include <nosSysVulkan/nosVulkanSubsystem.h>

NOS_INIT()
NOS_VULKAN_INIT()

NOS_BEGIN_IMPORT_DEPS()
	NOS_VULKAN_IMPORT()
NOS_END_IMPORT_DEPS()

namespace nos::compositing
{
nosResult RegisterBoxFit(nosNodeFunctions*);
nosResult RegisterChannelViewer(nosNodeFunctions*);
nosResult RegisterMerge(nosNodeFunctions*);
nosResult RegisterLayoutDrawer(nosNodeFunctions*);
nosResult RegisterFreeLayout(nosNodeFunctions*);
nosResult RegisterGridLayout(nosNodeFunctions*);
nosResult RegisterFreeOutputLayout(nosNodeFunctions*);
nosResult RegisterGridOutputLayout(nosNodeFunctions*);
void RegisterTextureTransitionNode(nosNodeFunctions*);
void RegisterTextureMapperNode(nosNodeFunctions*);
void RegisterMixerNode(nosNodeFunctions*);
void RegisterGrid3DNode(nosNodeFunctions*);
void RegisterCanvasMapperNode(nosNodeFunctions*);
}

namespace nos::compositing
{
static nosResult RegisterTextureTransition(nosNodeFunctions* nodeFunctions)
{
	RegisterTextureTransitionNode(nodeFunctions);
	return NOS_RESULT_SUCCESS;
}

static nosResult RegisterTextureMapper(nosNodeFunctions* nodeFunctions)
{
	RegisterTextureMapperNode(nodeFunctions);
	return NOS_RESULT_SUCCESS;
}

static nosResult RegisterMixer(nosNodeFunctions* nodeFunctions)
{
	RegisterMixerNode(nodeFunctions);
	return NOS_RESULT_SUCCESS;
}

static nosResult RegisterGrid3D(nosNodeFunctions* nodeFunctions)
{
	RegisterGrid3DNode(nodeFunctions);
	return NOS_RESULT_SUCCESS;
}

static nosResult RegisterCanvasMapper(nosNodeFunctions* nodeFunctions)
{
	RegisterCanvasMapperNode(nodeFunctions);
	return NOS_RESULT_SUCCESS;
}

enum class Nodes : size_t
{
	BoxFit,
	ChannelViewer,
	Merge,
	LayoutDrawer,
	FreeLayout,
	GridLayout,
	FreeOutputLayout,
	GridOutputLayout,
	TextureTransition,
	TextureMapper,
	Mixer,
	Grid3D,
	CanvasMapper,
	Count,
};

nosResult NOSAPI_CALL ExportNodeFunctions(size_t* outCount, nosNodeFunctions** outList)
{
	if (outCount)
		*outCount = static_cast<size_t>(Nodes::Count);
	if (!outList)
		return NOS_RESULT_SUCCESS;

#define GEN_CASE_NODE(name)            \
	case Nodes::name: {                \
		auto ret = Register##name(node); \
		if (ret != NOS_RESULT_SUCCESS)  \
			return ret;                 \
		break;                          \
	}

	for (size_t i = 0; i < static_cast<size_t>(Nodes::Count); ++i)
	{
		auto* node = outList[i];
		switch (static_cast<Nodes>(i))
		{
		default:
			break;
			GEN_CASE_NODE(BoxFit)
			GEN_CASE_NODE(ChannelViewer)
			GEN_CASE_NODE(Merge)
			GEN_CASE_NODE(LayoutDrawer)
			GEN_CASE_NODE(FreeLayout)
			GEN_CASE_NODE(GridLayout)
			GEN_CASE_NODE(FreeOutputLayout)
			GEN_CASE_NODE(GridOutputLayout)
			GEN_CASE_NODE(TextureTransition)
			GEN_CASE_NODE(TextureMapper)
			GEN_CASE_NODE(Mixer)
			GEN_CASE_NODE(Grid3D)
			GEN_CASE_NODE(CanvasMapper)
		}
	}

#undef GEN_CASE_NODE

	return NOS_RESULT_SUCCESS;
}

void GetRenamedTypes(nosName* outFrom, nosName* outTo, size_t* outSize)
{
	static std::vector<std::pair<nos::Name, nos::Name>> renames = {
		{NOS_NAME("nos.utilities.CanvasLayer"), NOS_NAME("nos.compositing.CanvasLayer")},
		{NOS_NAME("nos.utilities.MixerChannelType"), NOS_NAME("nos.compositing.MixerChannelType")},
		{NOS_NAME("nos.utilities.MixerTransitionType"), NOS_NAME("nos.compositing.MixerTransitionType")},
		{NOS_NAME("nos.utilities.MixerTransitionTarget"), NOS_NAME("nos.compositing.MixerTransitionTarget")},
		{NOS_NAME("nos.utilities.TransitionType"), NOS_NAME("nos.compositing.TransitionType")},
		{NOS_NAME("nos.utilities.TransitionInterpolation"), NOS_NAME("nos.compositing.TransitionInterpolation")},
		{NOS_NAME("nos.utilities.TransitionTarget"), NOS_NAME("nos.compositing.TransitionTarget")},
		{NOS_NAME("nos.utilities.BoxFitMode"), NOS_NAME("nos.compositing.BoxFitMode")},
		{NOS_NAME("nos.utilities.BlendMode"), NOS_NAME("nos.compositing.BlendMode")},
		{NOS_NAME("nos.utilities.Source"), NOS_NAME("nos.compositing.Source")},
		{NOS_NAME("nos.utilities.Channel"), NOS_NAME("nos.compositing.Channel")},
		{NOS_NAME("nos.utilities.ChannelViewerChannels"), NOS_NAME("nos.compositing.ChannelViewerChannels")},
		{NOS_NAME("nos.utilities.ChannelViewerFormats"), NOS_NAME("nos.compositing.ChannelViewerFormats")},
		{NOS_NAME("nos.utilities.TextureSwitcherChannel"), NOS_NAME("nos.compositing.TextureSwitcherChannel")},
		{NOS_NAME("nos.utilities.layout.LayoutType"), NOS_NAME("nos.compositing.LayoutType")},
		{NOS_NAME("nos.utilities.layout.FreeLayoutItem"), NOS_NAME("nos.compositing.FreeLayoutItem")},
		{NOS_NAME("nos.utilities.layout.GridLayoutItem"), NOS_NAME("nos.compositing.GridLayoutItem")},
		{NOS_NAME("nos.utilities.layout.FreeOutputItem"), NOS_NAME("nos.compositing.FreeOutputItem")},
		{NOS_NAME("nos.utilities.layout.GridOutputItem"), NOS_NAME("nos.compositing.GridOutputItem")},
		{NOS_NAME("nos.utilities.layout.LayoutDrawItem"), NOS_NAME("nos.compositing.LayoutDrawItem")},
		{NOS_NAME("nos.utilities.layout.LayoutOutputInfo"), NOS_NAME("nos.compositing.LayoutOutputInfo")},
		{NOS_NAME("nos.compositing.layout.LayoutType"), NOS_NAME("nos.compositing.LayoutType")},
		{NOS_NAME("nos.compositing.layout.FreeLayoutItem"), NOS_NAME("nos.compositing.FreeLayoutItem")},
		{NOS_NAME("nos.compositing.layout.GridLayoutItem"), NOS_NAME("nos.compositing.GridLayoutItem")},
		{NOS_NAME("nos.compositing.layout.FreeOutputItem"), NOS_NAME("nos.compositing.FreeOutputItem")},
		{NOS_NAME("nos.compositing.layout.GridOutputItem"), NOS_NAME("nos.compositing.GridOutputItem")},
		{NOS_NAME("nos.compositing.layout.LayoutDrawItem"), NOS_NAME("nos.compositing.LayoutDrawItem")},
		{NOS_NAME("nos.compositing.layout.LayoutOutputInfo"), NOS_NAME("nos.compositing.LayoutOutputInfo")},
		{NOS_NAME("nos.plugin.switcher.TextureSwitcherChannel"), NOS_NAME("nos.compositing.TextureSwitcherChannel")},
		{NOS_NAME("zd.utilities.BlendMode"), NOS_NAME("nos.compositing.BlendMode")},
		{NOS_NAME("zd.utilities.CanvasLayer"), NOS_NAME("nos.compositing.CanvasLayer")},
		{NOS_NAME("zd.utilities.MixerChannelType"), NOS_NAME("nos.compositing.MixerChannelType")},
		{NOS_NAME("zd.utilities.MixerTransitionType"), NOS_NAME("nos.compositing.MixerTransitionType")},
		{NOS_NAME("zd.utilities.MixerTransitionTarget"), NOS_NAME("nos.compositing.MixerTransitionTarget")},
		{NOS_NAME("zd.utilities.TransitionType"), NOS_NAME("nos.compositing.TransitionType")},
		{NOS_NAME("zd.utilities.TransitionInterpolation"), NOS_NAME("nos.compositing.TransitionInterpolation")},
		{NOS_NAME("zd.utilities.TransitionTarget"), NOS_NAME("nos.compositing.TransitionTarget")},
		{NOS_NAME("zd.utilities.BoxFitMode"), NOS_NAME("nos.compositing.BoxFitMode")},
		{NOS_NAME("zd.utilities.Source"), NOS_NAME("nos.compositing.Source")},
		{NOS_NAME("zd.utilities.Channel"), NOS_NAME("nos.compositing.Channel")},
		{NOS_NAME("zd.utilities.ChannelViewerChannels"), NOS_NAME("nos.compositing.ChannelViewerChannels")},
		{NOS_NAME("zd.utilities.ChannelViewerFormats"), NOS_NAME("nos.compositing.ChannelViewerFormats")},
		{NOS_NAME("zd.utilities.TextureSwitcherChannel"), NOS_NAME("nos.compositing.TextureSwitcherChannel")},
		{NOS_NAME("zd.utilities.layout.LayoutType"), NOS_NAME("nos.compositing.LayoutType")},
		{NOS_NAME("zd.utilities.layout.FreeLayoutItem"), NOS_NAME("nos.compositing.FreeLayoutItem")},
		{NOS_NAME("zd.utilities.layout.GridLayoutItem"), NOS_NAME("nos.compositing.GridLayoutItem")},
		{NOS_NAME("zd.utilities.layout.FreeOutputItem"), NOS_NAME("nos.compositing.FreeOutputItem")},
		{NOS_NAME("zd.utilities.layout.GridOutputItem"), NOS_NAME("nos.compositing.GridOutputItem")},
		{NOS_NAME("zd.utilities.layout.LayoutDrawItem"), NOS_NAME("nos.compositing.LayoutDrawItem")},
		{NOS_NAME("zd.utilities.layout.LayoutOutputInfo"), NOS_NAME("nos.compositing.LayoutOutputInfo")},
	};

	if (!outFrom)
	{
		*outSize = renames.size();
		return;
	}

	for (size_t i = 0; i < renames.size(); ++i)
	{
		outFrom[i] = renames[i].first;
		outTo[i] = renames[i].second;
	}
}

void GetRenamedNodeClasses(nosName* outFrom, nosName* outTo, size_t* outSize)
{
	static std::vector<std::pair<nos::Name, nos::Name>> renames = {
		{NOS_NAME("nos.utilities.BoxFit"), NOS_NAME("nos.compositing.BoxFit")},
		{NOS_NAME("nos.utilities.ChannelViewer"), NOS_NAME("nos.compositing.ChannelViewer")},
		{NOS_NAME("nos.utilities.Merge"), NOS_NAME("nos.compositing.Merge")},
		{NOS_NAME("nos.utilities.QuadMerge"), NOS_NAME("nos.compositing.QuadMerge")},
		{NOS_NAME("nos.utilities.Swizzle"), NOS_NAME("nos.compositing.Swizzle")},
		{NOS_NAME("nos.utilities.Layout"), NOS_NAME("nos.compositing.Layout")},
		{NOS_NAME("nos.utilities.LayoutDrawer"), NOS_NAME("nos.compositing.LayoutDrawer")},
		{NOS_NAME("nos.utilities.FreeLayout"), NOS_NAME("nos.compositing.FreeLayout")},
		{NOS_NAME("nos.utilities.GridLayout"), NOS_NAME("nos.compositing.GridLayout")},
		{NOS_NAME("nos.utilities.FreeOutputLayout"), NOS_NAME("nos.compositing.FreeOutputLayout")},
		{NOS_NAME("nos.utilities.GridOutputLayout"), NOS_NAME("nos.compositing.GridOutputLayout")},
		{NOS_NAME("nos.utilities.LayoutQuadCanvas"), NOS_NAME("nos.compositing.LayoutQuadCanvas")},
		{NOS_NAME("nos.utilities.TextureSwitcher"), NOS_NAME("nos.compositing.TextureSwitcher")},
		{NOS_NAME("nos.utilities.Offset"), NOS_NAME("nos.compositing.Offset")},
		{NOS_NAME("nos.utilities.SplitWipe"), NOS_NAME("nos.compositing.SplitWipe")},
		{NOS_NAME("nos.utilities.CanvasMapper"), NOS_NAME("nos.compositing.CanvasMapper")},
		{NOS_NAME("nos.utilities.Grid3D"), NOS_NAME("nos.compositing.Grid3D")},
		{NOS_NAME("nos.utilities.Mask"), NOS_NAME("nos.compositing.Mask")},
		{NOS_NAME("nos.utilities.Mixer"), NOS_NAME("nos.compositing.Mixer")},
		{NOS_NAME("nos.utilities.TextureMapper"), NOS_NAME("nos.compositing.TextureMapper")},
		{NOS_NAME("nos.utilities.TextureTransition"), NOS_NAME("nos.compositing.TextureTransition")},
		{NOS_NAME("zd.utilities.CanvasMapper"), NOS_NAME("nos.compositing.CanvasMapper")},
		{NOS_NAME("zd.utilities.Grid3D"), NOS_NAME("nos.compositing.Grid3D")},
		{NOS_NAME("zd.utilities.Mask"), NOS_NAME("nos.compositing.Mask")},
		{NOS_NAME("zd.utilities.Mixer"), NOS_NAME("nos.compositing.Mixer")},
		{NOS_NAME("zd.utilities.TextureMapper"), NOS_NAME("nos.compositing.TextureMapper")},
		{NOS_NAME("zd.utilities.TextureTransition"), NOS_NAME("nos.compositing.TextureTransition")},
	};

	if (!outFrom)
	{
		*outSize = renames.size();
		return;
	}

	for (size_t i = 0; i < renames.size(); ++i)
	{
		outFrom[i] = renames[i].first;
		outTo[i] = renames[i].second;
	}
}

extern "C"
{
NOSAPI_ATTR nosResult NOSAPI_CALL nosExportPlugin(nosPluginFunctions* out)
{
	out->ExportNodeFunctions = ExportNodeFunctions;
	out->GetRenamedTypes = GetRenamedTypes;
	out->GetRenamedNodeClasses = GetRenamedNodeClasses;
	return NOS_RESULT_SUCCESS;
}
}

} // namespace nos::compositing
