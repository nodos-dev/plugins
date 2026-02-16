// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

// Includes
#include <Nodos/PluginAPI.h>
#include <Nodos/Name.hpp>
#include <glm/glm.hpp>

// Dependencies
#include <nosSysVulkan/nosVulkanSubsystem.h>

NOS_INIT()
NOS_VULKAN_INIT()

NOS_BEGIN_IMPORT_DEPS()
	NOS_VULKAN_IMPORT()
NOS_END_IMPORT_DEPS()

namespace nos::filters
{

enum Filters : int
{
	Count
};

void GetRenamedNodeClasses(nosName* outRenamedFrom, nosName* outRenamedTo, size_t* outSize)
{
    static std::vector<std::pair<nos::Name, nos::Name>> renames = {
        {NOS_NAME("zd.utilities.3WayColorCorrect"), NOS_NAME("nos.filters.3WayColorCorrect")},
        {NOS_NAME("zd.utilities.ColorMatrix"), NOS_NAME("nos.filters.ColorMatrix")},
        {NOS_NAME("zd.utilities.Crop"), NOS_NAME("nos.filters.Crop")},
        {NOS_NAME("zd.utilities.Premultiply"), NOS_NAME("nos.filters.Premultiply")},
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

NOSAPI_ATTR nosResult NOSAPI_CALL ExportNodeFunctions(size_t* outSize, nosNodeFunctions** outList)
{
	if (!outList)
		*outSize = Filters::Count;
	return NOS_RESULT_SUCCESS;
}

extern "C"
{
NOSAPI_ATTR nosResult NOSAPI_CALL nosExportPlugin(nosPluginFunctions* outFunctions)
{
	outFunctions->ExportNodeFunctions = ExportNodeFunctions;
    outFunctions->GetRenamedNodeClasses = GetRenamedNodeClasses;
	return NOS_RESULT_SUCCESS;
}
}
}
