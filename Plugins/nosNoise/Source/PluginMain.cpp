// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

// Includes
#include <Nodos/PluginAPI.h>
#include <Nodos/Plugin.hpp>

// Subsystem dependencies
#include <nosVulkanSubsystem/Helpers.hpp>

NOS_INIT()
NOS_VULKAN_INIT()

NOS_BEGIN_IMPORT_DEPS()
	NOS_VULKAN_IMPORT()
NOS_END_IMPORT_DEPS()

namespace nos::noise
{
enum Nodes : int
{
    SimplexNoise = 0,
    WorleyNoise = 1,
    FBMNoise = 2,
    Count
};

struct DynamicSizedNoiseNode : public NodeContext
{
    nosResult ExecuteNode(NodeExecuteParams const& params) override
    {
		NodeExecuteParams execParams(params);
        auto outTex = execParams.GetPinObject<sys::vulkan::Texture>(NOS_NAME("Output"));
        auto& res = *execParams.GetPinData<const fb::vec2u>(NOS_NAME("Resolution"));
		auto outTexInfo = *sys::vulkan::GetResourceInfo(outTex);

        if (res.x() != outTexInfo.Width || res.y() != outTexInfo.Height)
        {
            outTexInfo.Width = res.x();
			outTexInfo.Height = res.y();
            nosEngine.LogD("Resizing texture to %dx%d", res.x(), res.y());
			SetPinObject(NOS_NAME("Output"), sys::vulkan::CreateTexture(outTexInfo, "DynamicSizedNoiseResult"));
        }
        return nosVulkan->ExecuteGPUNode(this, params.RawParams);
    }

};

nosResult NOSAPI_CALL ExportNodeFunctions(size_t* outSize, nosNodeFunctions** outList)
{
	if (!outList)
	{
		*outSize = Nodes::Count;
		return NOS_RESULT_SUCCESS;
	}

	NOS_BIND_NODE_CLASS(NOS_NAME("SimplexNoise"), DynamicSizedNoiseNode, outList[SimplexNoise])
	NOS_BIND_NODE_CLASS(NOS_NAME("WorleyNoise"), DynamicSizedNoiseNode, outList[WorleyNoise])
	NOS_BIND_NODE_CLASS(NOS_NAME("FBMNoise"), DynamicSizedNoiseNode, outList[FBMNoise]);
	return NOS_RESULT_SUCCESS;
}

extern "C"
{
NOSAPI_ATTR nosResult NOSAPI_CALL nosExportPlugin(nosPluginFunctions* outFunctions)
{
	outFunctions->ExportNodeFunctions = ExportNodeFunctions;
	return NOS_RESULT_SUCCESS;
}
}
} // namespace nos::noise