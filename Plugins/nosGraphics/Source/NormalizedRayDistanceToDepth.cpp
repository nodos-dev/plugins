// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/PluginHelpers.hpp>
#include <Graphics_generated.h>
#include <nosVulkanSubsystem/Helpers.hpp>
#include <glm/glm.hpp>

namespace nos::graphics
{
NOS_REGISTER_NAME(NormalizedRayDistanceToDepth)

NOS_REGISTER_NAME(InDepth)
NOS_REGISTER_NAME(RenderView)
NOS_REGISTER_NAME(Scale)
NOS_REGISTER_NAME(OutDepth)

NOS_REGISTER_NAME(NormalizedRayDistanceToDepth_Pass)
NOS_REGISTER_NAME(NormalizedRayDistanceToDepth_Frag)

struct NormalizedRayDistanceToDepth : NodeContext
{
	using NodeContext::NodeContext;

	nosResult ExecuteNode(nosNodeExecuteParams* params) override
	{
		NodeExecuteParams pins(params);

		auto inputTexture = vkss::DeserializeTextureInfo(pins[NSN_InDepth].Data->Data);
		TRenderView view = pins.GetPinData<TRenderView>(NSN_RenderView);
		if (!view.projection)
		{
			nosEngine.LogW("NormalizedRayDistanceToDepth requires a valid RenderView projection.");
			return NOS_RESULT_FAILED;
		}

		float scale = *pins.GetPinData<float>(NSN_Scale);
		glm::mat4 projection = reinterpret_cast<glm::mat4 const&>(view.left_handed_projection_matrix);
		glm::mat4 inverseProjection = glm::inverse(projection);
		glm::vec2 clipPlanes = reinterpret_cast<glm::vec2 const&>(view.projection->clip_planes);

		nosResourceShareInfo outputDepth = vkss::DeserializeTextureInfo(pins[NSN_OutDepth].Data->Data);

		auto tmpOutput = vkss::Resource::Create(
			nosTextureInfo{.Width = outputDepth.Info.Texture.Width,
						   .Height = outputDepth.Info.Texture.Height,
						   .Format = NOS_FORMAT_R8_UNORM,
						   .Filter = NOS_TEXTURE_FILTER_LINEAR,
						   .Usage = NOS_IMAGE_USAGE_RENDER_TARGET},
			"NormalizedRayDistanceToDepth Temp Output");
		if (!tmpOutput)
		{
			nosEngine.LogE("Failed to create temporary render target for NormalizedRayDistanceToDepth.");
			return NOS_RESULT_FAILED;
		}

		std::vector<nosShaderBinding> bindings;
		bindings.emplace_back(vkss::ShaderBinding(NOS_NAME("Input"), inputTexture));
		bindings.emplace_back(vkss::ShaderBinding(NOS_NAME("InverseProjection"), inverseProjection));
		bindings.emplace_back(vkss::ShaderBinding(NSN_Scale, scale));
		bindings.emplace_back(vkss::ShaderBinding(NOS_NAME("ClipNear"), clipPlanes.x));
		bindings.emplace_back(vkss::ShaderBinding(NOS_NAME("ClipFar"), clipPlanes.y));

		nosCmd cmd = {};
		nosCmdBeginParams beginParams{.Name = NOS_NAME("Normalized Ray Distance To Depth"),
									  .AssociatedNodeId = NodeId,
									  .OutCmdHandle = &cmd};
		nosVulkan->Begin(&beginParams);

		nosRunPassParams pass = {};
		pass.Key = NSN_NormalizedRayDistanceToDepth_Pass;
		pass.Bindings = bindings.data();
		pass.BindingCount = bindings.size();
		pass.Output = *tmpOutput;
		pass.Benchmark = 0;
		pass.DoNotClear = false;
		pass.DepthAttachment = {
			.DepthBuffer = outputDepth,
			.DoNotClear = false,
			.ClearValue = 1.0f,
		};
		pass.Vertices = {
			.IndexCount = 6,
			.DepthFunc = NOS_DEPTH_FUNCTION_ALWAYS,
			.DepthWrite = true,
			.DepthTest = true,
		};

		nosVulkan->RunPass(cmd, &pass);
		nosVulkan->End(cmd, nullptr);
		return NOS_RESULT_SUCCESS;
	}
};

nosResult RegisterNormalizedRayDistanceToDepth(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NSN_NormalizedRayDistanceToDepth, NormalizedRayDistanceToDepth, fn);

	fs::path root = nosEngine.Module->RootFolderPath;
	auto fragPath = (root / "Shaders" / "NormalizedRayDistanceToDepth.frag").generic_string();

	nosShaderInfo shader{
		.ShaderName = NSN_NormalizedRayDistanceToDepth_Frag,
		.Source = {.Stage = NOS_SHADER_STAGE_FRAG, .GLSLPath = fragPath.c_str()},
		.AssociatedNodeClassName = NSN_NormalizedRayDistanceToDepth,
	};

	auto ret = nosVulkan->RegisterShaders(1, &shader);
	if (ret != NOS_RESULT_SUCCESS)
		return ret;

	nosPassInfo pass = {
		.Key = NSN_NormalizedRayDistanceToDepth_Pass,
		.Shader = NSN_NormalizedRayDistanceToDepth_Frag,
		.MultiSample = 1,
	};

	return nosVulkan->RegisterPasses(1, &pass);
}
} // namespace nos::graphics
