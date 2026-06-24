// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>
#include <nosGraphics/Graphics_generated.h>
#include <nosSysVulkan/Helpers.hpp>
#include <glm/glm.hpp>

namespace nos::graphics
{
NOS_REGISTER_NAME(NormalizedDepthToDepthBuffer)

NOS_REGISTER_NAME(InDepth)
NOS_REGISTER_NAME(RenderView)
NOS_REGISTER_NAME(Scale)
NOS_REGISTER_NAME(OutDepth)

NOS_REGISTER_NAME(NormalizedDepthToDepthBuffer_Pass)
NOS_REGISTER_NAME(NormalizedDepthToDepthBuffer_Frag)

struct NormalizedDepthToDepthBuffer : NodeContext
{
	nos::ObjectRef TempColorOutput;
	uint32_t TempW = 0, TempH = 0;

	using NodeContext::NodeContext;

	nosResult ExecuteNode(nos::NodeExecuteParams const& pins) override
	{

		TRenderView view = pins.GetPinValue<TRenderView>(NSN_RenderView);
		if (!view.projection)
		{
			nosEngine.LogW("NormalizedDepthToDepthBuffer requires a valid RenderView projection.");
			return NOS_RESULT_FAILED;
		}

		float scale = *pins.GetPinValue<float>(NSN_Scale);
		glm::vec2 clipPlanes = reinterpret_cast<glm::vec2 const&>(view.projection->clip_planes);

		// The output depth buffer is an engine-managed resource on the OutDepth pin.
		auto outDepthInfo = nos::sys::vulkan::GetResourceInfo(pins.GetPinObject(NSN_OutDepth));
		if (!outDepthInfo)
		{
			nosEngine.LogW("NormalizedDepthToDepthBuffer: OutDepth pin has no resource yet.");
			return NOS_RESULT_FAILED;
		}
		uint32_t w = outDepthInfo->Texture.Width, h = outDepthInfo->Texture.Height;

		// A render pass needs a color attachment; this throwaway target sized to the
		// depth buffer satisfies that while the actual result is written to OutDepth.
		if (!TempColorOutput || TempW != w || TempH != h)
		{
			nosResourceInfo info = {};
			info.Type = NOS_RESOURCE_TYPE_TEXTURE;
			info.Texture.Width = w;
			info.Texture.Height = h;
			info.Texture.Format = NOS_FORMAT_R8_UNORM;
			info.Texture.Usage = nosImageUsage(NOS_IMAGE_USAGE_RENDER_TARGET | NOS_IMAGE_USAGE_SAMPLED);
			TempColorOutput = {};
			nosVulkan->CreateResource(&info, 0, "NormalizedDepthToDepthBuffer Temp Output", &TempColorOutput.GetStorage());
			if (!TempColorOutput)
			{
				nosEngine.LogW("NormalizedDepthToDepthBuffer: failed to create temp color target.");
				return NOS_RESULT_FAILED;   // leave TempW/TempH unchanged so creation retries next frame
			}
			TempW = w; TempH = h;
		}

		std::vector<nosShaderBinding> bindings = {
			nos::sys::vulkan::ShaderTextureBinding(NOS_NAME("Input"), pins.GetPinObject(NSN_InDepth), NOS_TEXTURE_FILTER_LINEAR),
			nos::sys::vulkan::ShaderDataBinding(NSN_Scale, scale),
			nos::sys::vulkan::ShaderDataBinding(NOS_NAME("ClipNear"), clipPlanes.x),
			nos::sys::vulkan::ShaderDataBinding(NOS_NAME("ClipFar"), clipPlanes.y),
		};

		nosCmd cmd = {};
		nosCmdBeginParams beginParams{.Name = NOS_NAME("Normalized Depth To Depth Buffer"),
									  .AssociatedNodeId = NodeId,
									  .OutCmdHandle = &cmd};
		nosVulkan->Begin(&beginParams);

		nosRunPassParams pass = {};
		pass.Key = NSN_NormalizedDepthToDepthBuffer_Pass;
		pass.Bindings = bindings.data();
		pass.BindingCount = bindings.size();
		pass.Output = TempColorOutput;
		pass.Benchmark = 0;
		pass.DoNotClear = false;
		pass.DepthAttachment = {
			.DepthBuffer = pins.GetPinObject(NSN_OutDepth),
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

nosResult RegisterNormalizedDepthToDepthBuffer(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NSN_NormalizedDepthToDepthBuffer, NormalizedDepthToDepthBuffer, fn);

	fs::path root = nosEngine.Plugin->RootFolderPath;
	auto fragPath = (root / "Shaders" / "NormalizedDepthToDepthBuffer.frag").generic_string();

	nosShaderInfo shader{
		.ShaderName = NSN_NormalizedDepthToDepthBuffer_Frag,
		.Source = {.Stage = NOS_SHADER_STAGE_FRAG, .GLSLPath = fragPath.c_str()},
		.AssociatedNodeClassName = NSN_NormalizedDepthToDepthBuffer,
	};

	auto ret = nosVulkan->RegisterShaders(1, &shader);
	if (ret != NOS_RESULT_SUCCESS)
		return ret;

	nosPassInfo pass = {
		.Key = NSN_NormalizedDepthToDepthBuffer_Pass,
		.Shader = NSN_NormalizedDepthToDepthBuffer_Frag,
		.MultiSample = 1,
	};

	return nosVulkan->RegisterPasses(1, &pass);
}
} // namespace nos::graphics
