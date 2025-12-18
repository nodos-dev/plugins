// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

// Includes
#include <Nodos/PluginHelpers.hpp>
#include <nosVulkanSubsystem/nosVulkanSubsystem.h>

NOS_INIT()
NOS_VULKAN_INIT()

NOS_BEGIN_IMPORT_DEPS()
NOS_VULKAN_IMPORT()
NOS_END_IMPORT_DEPS()

namespace nos::graphics
{
enum Nodes : int
{ // CPU nodes
	TrackToView,
	BillboardMask,
	Count
};

nosResult RegisterTrackToView(nosNodeFunctions*);
nosResult RegisterBillboardMask(nosNodeFunctions*);

struct PluginFunctions : nos::PluginFunctions
{
	nosResult NOSAPI_CALL ExportNodeFunctions(size_t& outSize, nosNodeFunctions** outFunctions)
	{
		outSize = Nodes::Count;
		if (!outFunctions)
			return NOS_RESULT_SUCCESS;
#define GEN_CASE_NODE(name)                                                                                            \
	case Nodes::name: {                                                                                                \
		auto ret = Register##name(node);                                                                               \
		if (NOS_RESULT_SUCCESS != ret)                                                                                 \
			return ret;                                                                                                \
		break;                                                                                                         \
	}
		for (int i = 0; i < Nodes::Count; ++i)
		{
			auto node = outFunctions[i];
			switch ((Nodes)i)
			{
			default: break; GEN_CASE_NODE(TrackToView) GEN_CASE_NODE(BillboardMask)
			}
		}
		return NOS_RESULT_SUCCESS;
	}
};
NOS_EXPORT_PLUGIN_FUNCTIONS(PluginFunctions)
} // namespace NOS_NAMESPACE