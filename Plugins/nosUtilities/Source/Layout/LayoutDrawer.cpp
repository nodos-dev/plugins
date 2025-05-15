// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/PluginHelpers.hpp>
#include <nosVulkanSubsystem/Helpers.hpp>
#include <glm/glm.hpp>
#include "Layout_generated.h"

NOS_REGISTER_NAME(LayoutDrawer)
NOS_REGISTER_NAME(TexturedQuad_Pass)
NOS_REGISTER_NAME(TexturedQuad_Frag)
NOS_REGISTER_NAME(TexturedQuad_Vert)
NOS_REGISTER_NAME(Output)
namespace nos::utilities
{
	inline nosResourceShareInfo DeserializeTextureInfo(sys::vulkan::Texture const& tex)
	{
		auto ext = tex.external_memory();
		return nosResourceShareInfo{
			.Memory =
				nosMemoryInfo{
					.Handle = tex.handle(),
					.Size = tex.size_in_bytes(),
					.ExternalMemory = {
						.HandleType = ext ? static_cast<nosExternalMemoryHandleType>(ext->handle_type()) : NOS_EXTERNAL_MEMORY_HANDLE_TYPE_NONE,
						.Handle = ext ? ext->handle() : 0,
						.Offset = tex.offset(),
						.AllocationSize = ext ? ext->allocation_size() : 0,
						.PID = ext ? ext->pid() : 0
					}
				},
			.Info =
				nosResourceInfo{
					.Type = NOS_RESOURCE_TYPE_TEXTURE,
					.Texture =
						nosTextureInfo{
							.Width = tex.width(),
							.Height = tex.height(),
							.Format = (nosFormat)tex.format(),
							.Filter = (nosTextureFilter)tex.filtering(),
							.Usage = (nosImageUsage)tex.usage(),
							.FieldType = (nosTextureFieldType)tex.field_type(),
						},
				},
		};
	}

	nosResult NOSAPI_CALL ExecuteLayoutDrawer(void* _, nosNodeExecuteParams* params)
	{
		nos::NodeExecuteParams args(params);
		auto const& layout = *args.GetPinData<layout::LayoutDrawList>(NOS_NAME("LayoutDrawList"));
		if (!layout.items())
			return NOS_RESULT_SUCCESS;
		
		auto outTex = vkss::DeserializeTextureInfo(args[NSN_Output].Data->Data);
		auto outSize = *args.GetPinData<nos::fb::vec2u>(NOS_NAME("OutputSize"));

		if (outTex.Info.Texture.Width != outSize.x() ||
			outTex.Info.Texture.Height != outSize.y())
		{
			outTex.Memory = {};
			outTex.Info.Texture.Width = outSize.x();
			outTex.Info.Texture.Height = outSize.y();
			nosEngine.SetPinValueByName(args.NodeId, NSN_Output, nos::Buffer::From(vkss::ConvertTextureInfo(outTex)));
			outTex = vkss::DeserializeTextureInfo(args[NSN_Output].Data->Data);
		}

		auto& inTexturesFb = *args.GetPinData<flatbuffers::Vector<flatbuffers::Offset<nos::sys::vulkan::Texture>>>(NOS_NAME("InputTextures"));
		std::vector<nosResourceShareInfo> inTextures;
		inTextures.reserve(inTexturesFb.size());
		for (auto* texture : inTexturesFb)
			inTextures.push_back(DeserializeTextureInfo(*texture));


		std::vector<nosRunPassParams> passes;
		bool first = true;
		auto cmd = vkss::BeginCmd(NOS_NAME("TextureCombiner"), args.NodeId);
		for (auto* item : *layout.items())
		{
			if (item->texture_id() >= inTextures.size())
			{
				nosEngine.LogE("Invalid texture id: %d", item->texture_id());
				continue;
			}
			auto const& inputTex = inTextures[item->texture_id()];
			auto pos = item->position();
			auto size = item->size();

			std::array bindings = { vkss::ShaderBinding(NOS_NAME("Offset"), pos),
								   vkss::ShaderBinding(NOS_NAME("Size"), size),
								   vkss::ShaderBinding(NOS_NAME("Input"), inputTex) };

			nosVertexData vertexData = {
				.DepthFunc = NOS_DEPTH_FUNCTION_ALWAYS,
				.DepthWrite = NOS_FALSE,
				.DepthTest = NOS_FALSE,
			};
			nosRunPassParams runPassParams{
				.Key = NSN_TexturedQuad_Pass,
				.Bindings = bindings.data(),
				.BindingCount = bindings.size(),
				.Output = outTex,
				.Vertices = vertexData,
				.Wireframe = NOS_FALSE,
				.Benchmark = NOS_FALSE,
				.DoNotClear = !first,
				.ClearCol = {0.0f, 0.0f, 0.0f, 1.0f}
			};
			nosVulkan->RunPass(cmd, &runPassParams);
			first = false;
		}
		vkss::EndCmd(cmd, false, nullptr);
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
