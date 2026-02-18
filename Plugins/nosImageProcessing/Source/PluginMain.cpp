// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>

#include <nosSysVulkan/nosVulkanSubsystem.h>

NOS_INIT()
NOS_VULKAN_INIT()

NOS_BEGIN_IMPORT_DEPS()
	NOS_VULKAN_IMPORT()
NOS_END_IMPORT_DEPS()

namespace nos::imageprocessing
{
nosResult RegisterResize(nosNodeFunctions*);
nosResult RegisterReduceTexture(nosNodeFunctions*);
void RegisterInterleaveNode(nosNodeFunctions*);
}

namespace nos::imageprocessing
{
static nosResult RegisterInterleave(nosNodeFunctions* nodeFunctions)
{
	RegisterInterleaveNode(nodeFunctions);
	return NOS_RESULT_SUCCESS;
}

enum class Nodes : size_t
{
	Resize,
	ReduceTexture,
	Interleave,
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
			GEN_CASE_NODE(Resize)
			GEN_CASE_NODE(ReduceTexture)
			GEN_CASE_NODE(Interleave)
		}
	}

#undef GEN_CASE_NODE

	return NOS_RESULT_SUCCESS;
}

void GetRenamedTypes(nosName* outFrom, nosName* outTo, size_t* outSize)
{
	static std::vector<std::pair<nos::Name, nos::Name>> renames = {
		{NOS_NAME("nos.utilities.ResizeMethod"), NOS_NAME("nos.imageprocessing.ResizeMethod")},
		{NOS_NAME("nos.fb.ResizeMethod"), NOS_NAME("nos.imageprocessing.ResizeMethod")},
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
		{NOS_NAME("nos.utilities.AutoResize"), NOS_NAME("nos.imageprocessing.AutoResize")},
		{NOS_NAME("nos.utilities.Resize"), NOS_NAME("nos.imageprocessing.Resize")},
		{NOS_NAME("nos.utilities.ReduceTexture"), NOS_NAME("nos.imageprocessing.ReduceTexture")},
		{NOS_NAME("nos.utilities.MeanSquaredError"), NOS_NAME("nos.imageprocessing.MeanSquaredError")},
		{NOS_NAME("nos.utilities.PSNR"), NOS_NAME("nos.imageprocessing.PSNR")},
		{NOS_NAME("nos.utilities.Interleave"), NOS_NAME("nos.imageprocessing.Interleave")},
		{NOS_NAME("zd.utilities.Interleave"), NOS_NAME("nos.imageprocessing.Interleave")},
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

} // namespace nos::imageprocessing
