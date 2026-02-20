// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>
#include <nosSysVulkan/nosVulkanSubsystem.h>

NOS_INIT()
NOS_VULKAN_INIT()

NOS_BEGIN_IMPORT_DEPS()
	NOS_VULKAN_IMPORT()
NOS_END_IMPORT_DEPS()

namespace nos::filters
{

enum class Nodes : size_t
{
	BoxBlur,
	DilateAlpha,
	DilateByAlpha,
	Cube3DLUT,
	TemporalBlur,
	Count,
};
void RegisterBoxBlurNode(nosNodeFunctions* nodeFunctions);
void RegisterDilateAlphaNode(nosNodeFunctions* nodeFunctions);
void RegisterDilateByAlphaNode(nosNodeFunctions* nodeFunctions);
void RegisterCube3DLUTNode(nosNodeFunctions* nodeFunctions);
void RegisterTemporalBlurNode(nosNodeFunctions* nodeFunctions);

nosResult NOSAPI_CALL ExportNodeFunctions(size_t* outSize, nosNodeFunctions** outList)
{
	if (outSize)
		*outSize = static_cast<size_t>(Nodes::Count);
	if (!outList)
		return NOS_RESULT_SUCCESS;

	for (size_t i = 0; i < static_cast<size_t>(Nodes::Count); ++i)
	{
		auto* node = outList[i];
		switch (static_cast<Nodes>(i))
		{
		case Nodes::BoxBlur:
			RegisterBoxBlurNode(node);
			break;
		case Nodes::DilateAlpha:
			RegisterDilateAlphaNode(node);
			break;
		case Nodes::DilateByAlpha:
			RegisterDilateByAlphaNode(node);
			break;
		case Nodes::Cube3DLUT:
			RegisterCube3DLUTNode(node);
			break;
		case Nodes::TemporalBlur:
			RegisterTemporalBlurNode(node);
			break;
		default:
			break;
		}
	}

	return NOS_RESULT_SUCCESS;
}

void GetRenamedTypes(nosName* outRenamedFrom, nosName* outRenamedTo, size_t* outSize)
{
	static std::vector<std::pair<nos::Name, nos::Name>> renames = {
		{NOS_NAME("zd.utilities.ColorspaceConversion"), NOS_NAME("nos.filters.ColorspaceConversion")},
		{NOS_NAME("zd.utilities.LUTType"), NOS_NAME("nos.filters.LUTType")},
	};

	if (!outRenamedFrom)
	{
		*outSize = renames.size();
		return;
	}

	for (size_t i = 0; i < renames.size(); ++i)
	{
		outRenamedFrom[i] = renames[i].first;
		outRenamedTo[i] = renames[i].second;
	}
}

void GetRenamedNodeClasses(nosName* outRenamedFrom, nosName* outRenamedTo, size_t* outSize)
{
	static std::vector<std::pair<nos::Name, nos::Name>> renames = {
		{NOS_NAME("nos.utilities.YADIF"), NOS_NAME("nos.filters.YADIF")},
		{NOS_NAME("nos.utilities.YADIFWithAutoDispatchSize"), NOS_NAME("nos.filters.YADIFWithAutoDispatchSize")},
		{NOS_NAME("zd.utilities.3WayColorCorrect"), NOS_NAME("nos.filters.3WayColorCorrect")},
		{NOS_NAME("zd.utilities.BoxBlur"), NOS_NAME("nos.filters.BoxBlur")},
		{NOS_NAME("zd.utilities.ColorMatrix"), NOS_NAME("nos.filters.ColorMatrix")},
		{NOS_NAME("zd.utilities.Crop"), NOS_NAME("nos.filters.Crop")},
		{NOS_NAME("zd.utilities.Cube3DLUT"), NOS_NAME("nos.filters.Cube3DLUT")},
		{NOS_NAME("zd.utilities.DilateAlpha"), NOS_NAME("nos.filters.DilateAlpha")},
		{NOS_NAME("zd.utilities.DilateByAlpha"), NOS_NAME("nos.filters.DilateByAlpha")},
		{NOS_NAME("zd.utilities.Premultiply"), NOS_NAME("nos.filters.Premultiply")},
		{NOS_NAME("zd.utilities.TemporalBlur"), NOS_NAME("nos.filters.TemporalBlur")},
		{NOS_NAME("zd.utilities.Unpremultiply"), NOS_NAME("nos.filters.Unpremultiply")},
	};

	if (!outRenamedFrom)
	{
		*outSize = renames.size();
		return;
	}

	for (size_t i = 0; i < renames.size(); ++i)
	{
		outRenamedFrom[i] = renames[i].first;
		outRenamedTo[i] = renames[i].second;
	}
}

extern "C"
{
NOSAPI_ATTR nosResult NOSAPI_CALL nosExportPlugin(nosPluginFunctions* outFunctions)
{
	outFunctions->ExportNodeFunctions = ExportNodeFunctions;
	outFunctions->GetRenamedTypes = GetRenamedTypes;
	outFunctions->GetRenamedNodeClasses = GetRenamedNodeClasses;
	return NOS_RESULT_SUCCESS;
}
}
} // namespace nos::filters
