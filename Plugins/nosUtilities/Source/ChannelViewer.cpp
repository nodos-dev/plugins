// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#include <Nodos/Plugin.hpp>

#include <nosVulkanSubsystem/Helpers.hpp>
#include <glm/glm.hpp>
#include "Names.h"

NOS_REGISTER_NAME(Channel);
NOS_REGISTER_NAME(Format);
NOS_REGISTER_NAME(Channel_Viewer_Pass);
NOS_REGISTER_NAME(Channel_Viewer_Shader);
NOS_REGISTER_NAME_SPACED(Nos_Utilities_ChannelViewer, "nos.utilities.ChannelViewer")

namespace nos::utilities
{
static nosResult ExecuteNode(void* ctx, nosNodeExecuteParams* execParams)
{
	NodeExecuteParams params(execParams);
	auto inTex = params.GetPinObject<vkss::Texture>(NSN_Input);
	auto outTex = params.GetPinObject<vkss::Texture>(NSN_Output);

	auto channel = *params.GetPinData<uint32_t>(NSN_Channel);
	auto format = *params.GetPinData<uint32_t>(NSN_Format);

	glm::vec4 val{};
	val[channel & 3] = 1;

	constexpr glm::vec3 coeffs[3] = {{.299f, .587f, .114f}, {.2126f, .7152f, .0722f}, {.2627f, .678f, .0593f}};

	glm::vec4 multipliers = glm::vec4(coeffs[format], channel > 3);
	std::vector bindings = {
		vkss::ShaderTextureBinding(NSN_Input, inTex, NOS_TEXTURE_FILTER_NEAREST),
		vkss::ShaderDataBinding(NSN_Channel, val), 
		vkss::ShaderDataBinding(NSN_Format, multipliers)
	};

	nosRunPassParams pass = {
		.Key = NSN_Channel_Viewer_Pass,
		.Bindings = bindings.data(),
		.BindingCount = (uint32_t)bindings.size(),
		.Output = outTex,
		.Wireframe = false,
	};
	auto cmd = vkss::BeginCmd(NOS_NAME("ChannelViewer"), params.NodeId);
	nosVulkan->RunPass(cmd, &pass);
	vkss::EndCmd(cmd, false, nullptr);
	return NOS_RESULT_SUCCESS;
}

nosResult RegisterChannelViewer(nosNodeFunctions* out)
{
	out->ClassName = NSN_Nos_Utilities_ChannelViewer;
	out->ExecuteNode = ExecuteNode;

	fs::path root = nosEngine.Plugin->RootFolderPath;
	auto chViewerPath = (root / "Shaders" / "ChannelViewer.frag").generic_string();

	nosShaderInfo shader = {
		.ShaderName = NSN_Channel_Viewer_Shader, .Source = {.Stage = NOS_SHADER_STAGE_FRAG, .GLSLPath = chViewerPath.c_str()},
		.AssociatedNodeClassName = NSN_Nos_Utilities_ChannelViewer
	};
	auto ret = nosVulkan->RegisterShaders(1, &shader);
	if (NOS_RESULT_SUCCESS != ret)
		return ret;

	nosPassInfo pass = {
		.Key = NSN_Channel_Viewer_Pass,
		.Shader = NSN_Channel_Viewer_Shader,
		.MultiSample = 1
	};
	return nosVulkan->RegisterPasses(1, &pass);
}
} 

