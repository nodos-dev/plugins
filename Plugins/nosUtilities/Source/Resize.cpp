// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>

#include <nosVulkanSubsystem/Helpers.hpp>

#include "Names.h"

namespace nos::utilities
{

NOS_REGISTER_NAME(RESIZE_PASS);
NOS_REGISTER_NAME(Method);
NOS_REGISTER_NAME(Size);
NOS_REGISTER_NAME_SPACED(Nos_Utilities_Resize, "nos.utilities.Resize")

static nosResult ExecuteNode(void* ctx, nosNodeExecuteParams* params)
{
	nos::NodeExecuteParams nodeParams(params);
	auto inTex = nodeParams.GetPinObject<vkss::Texture>(NSN_Input);
	auto method = *nodeParams.GetPinData<uint32_t>(NSN_Method);

	auto outTex = nodeParams.GetPinObject<vkss::Texture>(NSN_Output);
	auto& size = *nodeParams.GetPinData<nosVec2u>(NSN_Size);
		
	auto outTexInfo = vkss::GetResourceInfo(outTex);

	if(!outTexInfo || size.x != outTexInfo->Width || size.y != outTexInfo->Height)
	{
		auto newTexInfo = outTexInfo.value_or(nosTextureInfo{});
		newTexInfo.Width = size.x;
		newTexInfo.Height = size.y;
		// TODO: Transfer output unscaled
		outTex = vkss::CreateTexture(newTexInfo, "Resize Output");
		nosEngine.SetPinObjectHandle(params->Pins[1]->Id, outTex);
	}
    
	// TODO: Transfer filter
	std::vector bindings = {vkss::ShaderTextureBinding(NSN_Input, inTex, NOS_TEXTURE_FILTER_LINEAR), vkss::ShaderDataBinding(NSN_Method, method)};
	
	nosRunPassParams resizeParam {
		.Key = NSN_RESIZE_PASS,
		.Bindings = bindings.data(),
		.BindingCount = 2,
		.Output = outTex,
		.Wireframe = 0,
		.Benchmark = 0,
	};

	nosCmd cmd;
	nosCmdBeginParams beginParams {.Name = NOS_NAME("Resize"), .AssociatedNodeId = params->NodeId, .OutCmdHandle = &cmd};
	nosVulkan->Begin(&beginParams);
	nosVulkan->RunPass(cmd, &resizeParam);
	nosVulkan->End(cmd, nullptr);

	return NOS_RESULT_SUCCESS;
}

nosResult RegisterResize(nosNodeFunctions* out)
{
	out->ClassName = NSN_Nos_Utilities_Resize;
	out->ExecuteNode = ExecuteNode;
	return NOS_RESULT_SUCCESS;
}

} // namespace nos::utilities


