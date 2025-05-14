// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/PluginHelpers.hpp>
#include <nosVulkanSubsystem/nosVulkanSubsystem.h>

#include "Layout_generated.h"

NOS_REGISTER_NAME(LayoutDrawer)
NOS_REGISTER_NAME(TexturedQuad_Pass)
NOS_REGISTER_NAME(TexturedQuad_Frag)
NOS_REGISTER_NAME(TexturedQuad_Vert)
namespace nos::utilities
{
	nosResult NOSAPI_CALL ExecuteLayoutDrawer(void* _, nosNodeExecuteParams* params)
	{
		return NOS_RESULT_SUCCESS;
	}
	nosResult RegisterLayoutDrawer(nosNodeFunctions* fn)
	{
		fn->ClassName = NSN_LayoutDrawer;
		fn->ExecuteNode = ExecuteLayoutDrawer;


		fs::path root = nosEngine.Module->RootFolderPath;
		auto fragPath = (root / "Shaders" / "TexturedQuad.frag").generic_string();
		auto vertPath = (root / "Shaders" / "TexturedQuad.vert").generic_string();

		// Register shaders
		std::array shaders = {
			nosShaderInfo{.ShaderName = NSN_TexturedQuad_Frag,
									 .Source = {.Stage = NOS_SHADER_STAGE_FRAG, .GLSLPath = fragPath.c_str()},
									 .AssociatedNodeClassName = NSN_LayoutDrawer},
			nosShaderInfo{.ShaderName = NSN_TexturedQuad_Vert,
									 .Source = {.Stage = NOS_SHADER_STAGE_VERT, .GLSLPath = vertPath.c_str()},
									 .AssociatedNodeClassName = NSN_LayoutDrawer} };
		auto ret = nosVulkan->RegisterShaders(shaders.size(), shaders.data());
		if (NOS_RESULT_SUCCESS != ret)
			return ret;

		nosPassInfo pass = {
			.Key = NSN_TexturedQuad_Pass,
			.Shader = NSN_TexturedQuad_Frag,
			.VertexShader = NSN_TexturedQuad_Vert,
			.MultiSample = 1,
		};

		ret = nosVulkan->RegisterPasses(1, &pass);
		return ret;



		return NOS_RESULT_SUCCESS;
	}
}
