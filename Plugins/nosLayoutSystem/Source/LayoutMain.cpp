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

namespace nos::layout
{

enum Layout : int
{
	LayoutDrawer = 0,
	FreeLayout,
	GridLayout,
	FreeOutputLayout,
	GridOutputLayout,
	Count
};

// Forward declarations
nosResult RegisterLayoutDrawer(nosNodeFunctions*);
nosResult RegisterFreeLayout(nosNodeFunctions*);
nosResult RegisterGridLayout(nosNodeFunctions*);
nosResult RegisterFreeOutputLayout(nosNodeFunctions*);
nosResult RegisterGridOutputLayout(nosNodeFunctions*);

nosResult NOSAPI_CALL ExportNodeFunctions(size_t* outSize, nosNodeFunctions** outList)
{
	*outSize = Layout::Count;
	if (!outList)
		return NOS_RESULT_SUCCESS;

#define GEN_CASE_NODE(name)					\
	case Layout::name: {					\
		auto ret = Register##name(node);	\
		if (NOS_RESULT_SUCCESS != ret)		\
			return ret;						\
		break;								\
	}

	for (int i = 0; i < Layout::Count; ++i)
	{
		auto node = outList[i];
		switch ((Layout)i) {
		default:
			break;
			GEN_CASE_NODE(LayoutDrawer)
			GEN_CASE_NODE(FreeLayout)
			GEN_CASE_NODE(GridLayout)
			GEN_CASE_NODE(FreeOutputLayout)
			GEN_CASE_NODE(GridOutputLayout)
		}
	}
	
	return NOS_RESULT_SUCCESS;
}

extern "C"
{
NOSAPI_ATTR nosResult NOSAPI_CALL nosExportPlugin(nosPluginFunctions* out)
{
	out->ExportNodeFunctions = ExportNodeFunctions;
	return NOS_RESULT_SUCCESS;
}
}
}
