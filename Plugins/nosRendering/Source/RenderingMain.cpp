// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

// Includes
#include <Nodos/PluginHelpers.hpp>
#include <nosVulkanSubsystem/nosVulkanSubsystem.h>

#define NOS_NODES                                                                                                      \
	NOS_NODE_OP(TrackToView)                                                                                           \
	NOS_NODE_OP(BillboardMask)

#define NOS_DEPS NOS_DEP_OP(NOS_VULKAN)

#define NOS_NAMESPACE nos::rendering

namespace nos::rendering
{
struct PluginFunctions : nos::PluginFunctions
{
	nosResult NOSAPI_CALL ExportNodeFunctions(size_t& outSize, nosNodeFunctions** outFunctions) override;
};
} // namespace nos::rendering



// Nodos/PluginMain.inl

NOS_INIT()

#define NOS_DEP_OP(name) name##_INIT();

NOS_DEPS

#undef NOS_DEP_OP

#define NOS_DEP_OP(name) name##_IMPORT();

NOS_BEGIN_IMPORT_DEPS()
NOS_DEPS
NOS_END_IMPORT_DEPS()

#undef NOS_DEP_OP

namespace NOS_NAMESPACE
{
#define NOS_NODE_OP(name) name,

enum Nodes : int
{ // CPU nodes
	NOS_NODES Count
};

#undef NOS_NODE_OP
#define NOS_NODE_OP(name) nosResult Register##name(nosNodeFunctions*);
NOS_NODES

#undef NOS_NODE_OP

nosResult NOSAPI_CALL PluginFunctions::ExportNodeFunctions(size_t& outSize, nosNodeFunctions** outFunctions)
{
	outSize = Nodes::Count;
	if (!outFunctions)
		return NOS_RESULT_SUCCESS;
#define NOS_NODE_OP(name)                                                                                              \
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
		default: break; NOS_NODES
		}
	}
	return NOS_RESULT_SUCCESS;
}
NOS_EXPORT_PLUGIN_FUNCTIONS(PluginFunctions)
} // namespace NOS_NAMESPACE