// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/PluginHelpers.hpp>
#include <nosVulkanSubsystem/Helpers.hpp>
#include <glm/glm.hpp>
#include "Layout_generated.h"

NOS_REGISTER_NAME(LayoutDrawer)
NOS_REGISTER_NAME(TexturedQuad_Pass)
NOS_REGISTER_NAME(TexturedQuad_Frag)
NOS_REGISTER_NAME(TexturedQuad_Vert)
NOS_REGISTER_NAME(OutputTextures)
namespace nos::utilities
{
inline nosResourceShareInfo DeserializeTextureInfo(sys::vulkan::Texture const& tex)
{
	auto ext = tex.external_memory();
	return nosResourceShareInfo{
		.Memory = nosMemoryInfo{.Handle = tex.handle(),
								.Size = tex.size_in_bytes(),
								.ExternalMemory = {.HandleType = ext ? static_cast<nosExternalMemoryHandleType>(
																		   ext->handle_type())
																	 : NOS_EXTERNAL_MEMORY_HANDLE_TYPE_NONE,
												   .Handle = ext ? ext->handle() : 0,
												   .Offset = tex.offset(),
												   .AllocationSize = ext ? ext->allocation_size() : 0,
												   .PID = ext ? ext->pid() : 0}},
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

struct LayoutDrawerNode : NodeContext
{
	// This includes invalid textures too, check for handle before using. The output pin doesnt include invalid
	// textures.
	std::vector<nosResourceShareInfo> OutTextures;

	void UpdateOutputTexturesPin()
	{
		std::vector<flatbuffers::Offset<sys::vulkan::Texture>> newOutputsFb;
		flatbuffers::FlatBufferBuilder fbb;
		newOutputsFb.reserve(OutTextures.size());
		for (auto& tex : OutTextures)
		{
			if (tex.Memory.Handle == 0)
				continue;
			auto tTex = vkss::ConvertTextureInfo(tex);
			newOutputsFb.push_back(sys::vulkan::CreateTexture(fbb, &tTex));
		}

		auto vecOffset = fbb.CreateVector(newOutputsFb);
		fbb.Finish(vecOffset);
		auto buf = fbb.Release();
		auto vecs = flatbuffers::GetRoot<flatbuffers::Vector<flatbuffers::Offset<sys::vulkan::Texture>>>(buf.data());
		SetPinValue(NSN_OutputTextures,
					nosBuffer{.Data = (void*)vecs, .Size = buf.size() - ((uint8_t*)vecs - buf.data())});
	}

	LayoutDrawerNode(nosFbNodePtr node) : NodeContext(node) 
	{
		UpdateOutputTexturesPin();
	}
	~LayoutDrawerNode()
	{
		for (auto const& tex : OutTextures)
		{
			nosVulkan->DestroyResource(&tex);
		}
	}

	
	nosResult ExecuteNode(nosNodeExecuteParams* params) override
	{
		nos::NodeExecuteParams args(params);
		auto const& drawInfo = *args.GetPinData<layout::LayoutDrawInfo>(NOS_NAME("LayoutDrawInfo"));
		if (!drawInfo.draw_items())
			return NOS_RESULT_SUCCESS;

		auto outputs =
			args.GetPinData<flatbuffers::Vector<flatbuffers::Offset<nos::sys::vulkan::Texture>>>(NSN_OutputTextures);

		if (!drawInfo.outputs())
			return NOS_RESULT_SUCCESS;

		OutTextures.clear();
		bool outsChanged = false;
		for (size_t i = 0; i < std::min(outputs->size(), drawInfo.outputs()->size()); ++i)
		{
			auto tex = DeserializeTextureInfo(*(*outputs)[i]);
			auto const& outInfo = *(*drawInfo.outputs())[i];
			if (tex.Info.Texture.Width != outInfo.resolution().x() ||
				tex.Info.Texture.Height != outInfo.resolution().y())
			{
				nosVulkan->DestroyResource(&tex);
				tex.Memory = {};
				tex.Info.Texture.Width = outInfo.resolution().x();
				tex.Info.Texture.Height = outInfo.resolution().y();
				if (nosVulkan->CreateResource(&tex, "LayoutDrawerOut") != NOS_RESULT_SUCCESS)
					nosEngine.LogE("Failed to create output texture for output %ull!", i);
				outsChanged = true;
			}
			OutTextures.emplace_back(tex);
		}

		if (outputs->size() > drawInfo.outputs()->size())
		{
			outsChanged = true;
			for (size_t i = drawInfo.outputs()->size(); i < outputs->size(); ++i)
			{
				auto tex = DeserializeTextureInfo(*(*outputs)[i]);
				nosVulkan->DestroyResource(&tex);
			}
		}
		else
		{
			outsChanged = true;
			for (size_t i = outputs->size(); i < drawInfo.outputs()->size(); ++i)
			{
				auto& output = *(*drawInfo.outputs())[i];
				nosResourceShareInfo tex{};
				tex.Info.Type = NOS_RESOURCE_TYPE_TEXTURE;
				tex.Info.Texture = {
					.Width = output.resolution().x(),
					.Height = output.resolution().y(),
					.Format = NOS_FORMAT_R16G16B16A16_UNORM,
					.Usage = nosImageUsage(NOS_IMAGE_USAGE_SAMPLED | NOS_IMAGE_USAGE_RENDER_TARGET |
										   NOS_IMAGE_USAGE_TRANSFER_SRC | NOS_IMAGE_USAGE_TRANSFER_DST),
				};
				if (nosVulkan->CreateResource(&tex, "LayoutDrawerOut") != NOS_RESULT_SUCCESS)
					nosEngine.LogE("Failed to create output texture for output %ull!", i);
				OutTextures.emplace_back(tex);
			}
		}

		if (outsChanged)
		{
			UpdateOutputTexturesPin();
			outputs = args.GetPinData<flatbuffers::Vector<flatbuffers::Offset<nos::sys::vulkan::Texture>>>(
				NSN_OutputTextures);
		}

		auto& inTexturesFb = *args.GetPinData<flatbuffers::Vector<flatbuffers::Offset<nos::sys::vulkan::Texture>>>(
			NOS_NAME("InputTextures"));
		std::vector<nosResourceShareInfo> inTextures;
		inTextures.reserve(inTexturesFb.size());
		for (auto* texture : inTexturesFb)
			inTextures.push_back(DeserializeTextureInfo(*texture));

		auto cmd = vkss::BeginCmd(NOS_NAME("LayoutDrawer"), NodeId);
		for (size_t outIndex = 0; outIndex < drawInfo.outputs()->size(); outIndex++)
		{
			auto const& outInfo = *(*drawInfo.outputs())[outIndex];
			auto outTex = OutTextures[outIndex];
			if (outTex.Memory.Handle == 0)
				continue;
			nosVulkan->Clear(cmd, &outTex, {0.0f, 0.0f, 0.0f, 1.0f});

			auto outSize = outInfo.size();
			auto outPos = outInfo.pos();
			std::vector<layout::LayoutDrawItem> drawItemsForOut;
			drawItemsForOut.reserve(drawInfo.draw_items()->size());
			// Calculate translation and scale matrix of the output texture
			glm::vec2 translation = {outPos.x(), outPos.y()};
			glm::vec2 scale = {outSize.x(), outSize.y()};
			// Apply the transformation to each draw item
			for (auto* drawItem : *drawInfo.draw_items())
			{
				auto drawItemPos = drawItem->position();
				auto drawItemSize = drawItem->size();
				glm::vec2 pos = {drawItemPos.x(), drawItemPos.y()};
				glm::vec2 endPos = {drawItemPos.x() + drawItemSize.x(), drawItemPos.y() + drawItemSize.y()};

				glm::vec2 transformedPos = (pos - translation) / scale;
				glm::vec2 transformedEndPos = (endPos - translation) / scale;

				glm::vec2 newPos = {transformedPos.x, transformedPos.y};
				glm::vec2 newSize = {transformedEndPos.x - transformedPos.x, transformedEndPos.y - transformedPos.y};
				layout::LayoutDrawItem newItem{};
				newItem.mutable_position() = nos::fb::vec2(newPos.x, newPos.y);
				newItem.mutable_size() = nos::fb::vec2(newSize.x, newSize.y);
				newItem.mutate_texture_id(drawItem->texture_id());
				drawItemsForOut.emplace_back(newItem);
			}
			DrawOut(cmd, inTextures, drawItemsForOut, outTex);
		}
		vkss::EndCmd(cmd, false, nullptr);
		return NOS_RESULT_SUCCESS;
	}

	void DrawOut(nosCmd cmd,
				 std::span<nosResourceShareInfo> textures,
				 std::span<layout::LayoutDrawItem> drawList,
				 nosResourceShareInfo const& output)
	{
		for (auto const& item : drawList)
		{
			if (item.texture_id() >= textures.size())
			{
				nosEngine.LogE("Invalid texture id: %d", item.texture_id());
				continue;
			}
			auto const& inputTex = textures[item.texture_id()];
			auto pos = item.position();
			auto size = item.size();

			std::array bindings = {vkss::ShaderBinding(NOS_NAME("Offset"), pos),
								   vkss::ShaderBinding(NOS_NAME("Size"), size),
								   vkss::ShaderBinding(NOS_NAME("Input"), inputTex)};

			nosVertexData vertexData = {
				.DepthFunc = NOS_DEPTH_FUNCTION_ALWAYS,
				.DepthWrite = NOS_FALSE,
				.DepthTest = NOS_FALSE,
			};
			nosRunPassParams runPassParams{.Key = NSN_TexturedQuad_Pass,
										   .Bindings = bindings.data(),
										   .BindingCount = bindings.size(),
										   .Output = output,
										   .Vertices = vertexData,
										   .Wireframe = NOS_FALSE,
										   .Benchmark = NOS_FALSE,
										   .DoNotClear = true};
			nosVulkan->RunPass(cmd, &runPassParams);
		}
	}
};

nosResult RegisterLayoutDrawer(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NSN_LayoutDrawer, LayoutDrawerNode, fn);

	fs::path root = nosEngine.Module->RootFolderPath;
	auto fragPath = (root / "Shaders" / "TexturedQuad.frag").generic_string();
	auto vertPath = (root / "Shaders" / "TexturedQuad.vert").generic_string();

	// Register shaders
	std::array shaders = {nosShaderInfo{.ShaderName = NSN_TexturedQuad_Frag,
										.Source = {.Stage = NOS_SHADER_STAGE_FRAG, .GLSLPath = fragPath.c_str()},
										.AssociatedNodeClassName = NSN_LayoutDrawer},
						  nosShaderInfo{.ShaderName = NSN_TexturedQuad_Vert,
										.Source = {.Stage = NOS_SHADER_STAGE_VERT, .GLSLPath = vertPath.c_str()},
										.AssociatedNodeClassName = NSN_LayoutDrawer}};
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
} // namespace nos::utilities
