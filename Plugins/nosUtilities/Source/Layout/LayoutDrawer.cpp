// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>
#include <nosSysVulkan/Helpers.hpp>
#include <glm/glm.hpp>
#include "nosUtilities/Layout_generated.h"

NOS_REGISTER_NAME(LayoutDrawer)
NOS_REGISTER_NAME(TexturedQuad_Pass)
NOS_REGISTER_NAME(TexturedQuad_Frag)
NOS_REGISTER_NAME(TexturedQuad_Vert)
NOS_REGISTER_NAME(QuadOutline_Pass)
NOS_REGISTER_NAME(QuadOutline_Frag)
NOS_REGISTER_NAME(QuadOutline_Vert)
NOS_REGISTER_NAME(OutputTextures)
NOS_REGISTER_NAME(Preview)
namespace nos::utilities
{

struct LayoutDrawerNode : NodeContext
{
// TODO: Transfer
//	// This includes invalid textures too, check for handle before using. The output pin doesnt include invalid
//	// textures.
//	std::vector<TypedObjectRef<vkss::Texture>> OutTextures;
//
//	enum class StatusType
//	{
//		Preview,
//		InvalidInputTextures,
//	};
//	std::map<StatusType, fb::TNodeStatusMessage> StatusMessages;
//
//	void SendStatusMessages()
//	{
//		std::vector<fb::TNodeStatusMessage> messages;
//		for (auto& [type, message] : StatusMessages)
//			messages.push_back(message);
//		if (messages.empty())
//			ClearNodeStatusMessages();
//		else
//			SetNodeStatusMessages(messages);
//	}
//
//	void SetStatusMessage(StatusType statusType, std::string_view message, fb::NodeStatusMessageType msgType)
//	{
//		if (auto it = StatusMessages.find(statusType); it != StatusMessages.end())
//		{
//			if (it->second.text == message && it->second.type == msgType)
//				return;
//		}
//		auto& msg = StatusMessages[statusType] = fb::TNodeStatusMessage{};
//		msg.text = message;
//		msg.type = msgType;
//		SendStatusMessages();
//	}
//
//	void ClearStatusMessage(StatusType statusType)
//	{
//		auto it = StatusMessages.find(statusType);
//		if (it == StatusMessages.end())
//			return;
//		StatusMessages.erase(statusType);
//		SendStatusMessages();
//	}
//
//	void UpdateOutputTexturesPin()
//	{
//		std::vector<flatbuffers::Offset<sys::vulkan::Texture>> newOutputsFb;
//		flatbuffers::FlatBufferBuilder fbb;
//		newOutputsFb.reserve(OutTextures.size());
//		for (auto& tex : OutTextures)
//		{
//			if (tex.IsValid())
//				continue;
//			newOutputsFb.push_back(sys::vulkan::CreateTexture(fbb, &tTex));
//		}
//
//		auto vecOffset = fbb.CreateVector(newOutputsFb);
//		fbb.Finish(vecOffset);
//		auto buf = fbb.Release();
//		auto vecs = flatbuffers::GetRoot<flatbuffers::Vector<flatbuffers::Offset<sys::vulkan::Texture>>>(buf.data());
//		SetPinValue(NSN_OutputTextures,
//					nosBuffer{.Data = (void*)vecs, .Size = buf.size() - ((uint8_t*)vecs - buf.data())});
//	}
//
//	nosResult OnCreate(nosFbNodePtr node) override
//	{
//		UpdateOutputTexturesPin();
//		AddPinValueWatcher(
//			NOS_NAME("PreviewEnabled"), [this](const nos::Buffer& newVal, std::optional<nos::Buffer> oldValue) {
//				bool previewEnabled = *InterpretPinValue<bool>(newVal);
//				SetPinOrphanState(NSN_Preview,
//								  previewEnabled ? fb::PinOrphanStateType::ACTIVE : fb::PinOrphanStateType::ORPHAN,
//								  "Preview disabled.");
//				if (previewEnabled)
//					SetStatusMessage(StatusType::Preview, "Preview enabled.", fb::NodeStatusMessageType::INFO);
//				else
//					ClearStatusMessage(StatusType::Preview);
//			});
//		ClearNodeStatusMessages();
//		return NOS_RESULT_SUCCESS;
//	}
//	nosResult OnDestroy() override
//	{
//		for (auto const& tex : OutTextures)
//			nosVulkan->DestroyResource(&tex);
//		return NOS_RESULT_SUCCESS;
//	}
//
//	
//	nosResult ExecuteNode(nosNodeExecuteParams* params) override
//	{
//		nos::NodeExecuteParams args(params);
//		auto const& drawItems =
//			*args.GetPinData<flatbuffers::Vector<layout::LayoutDrawItem const*>>(NOS_NAME("LayoutDrawItems"));
//		auto const& outputInfos =
//			*args.GetPinData<flatbuffers::Vector<layout::LayoutOutputInfo const*>>(NOS_NAME("LayoutOutputInfos"));
//		auto* outputs =
//			args.GetPinData<flatbuffers::Vector<flatbuffers::Offset<nos::sys::vulkan::Texture>>>(NSN_OutputTextures);
//
//		OutTextures.clear();
//		bool outsChanged = false;
//		for (size_t i = 0; i < std::min(outputs->size(), outputInfos.size()); ++i)
//		{
//			auto tex = DeserializeTextureInfo(*(*outputs)[i]);
//			auto const& outInfo = *outputInfos[i];
//			if (tex.Info.Texture.Width != outInfo.resolution().x() ||
//				tex.Info.Texture.Height != outInfo.resolution().y())
//			{
//				nosVulkan->DestroyResource(&tex);
//				tex.Memory = {};
//				tex.Info.Texture.Width = outInfo.resolution().x();
//				tex.Info.Texture.Height = outInfo.resolution().y();
//				if (nosVulkan->CreateResource(&tex, "LayoutDrawerOut") != NOS_RESULT_SUCCESS)
//					nosEngine.LogE("Failed to create output texture for output %zu!", i);
//				outsChanged = true;
//			}
//			OutTextures.emplace_back(tex);
//		}
//
//		if (outputs->size() > outputInfos.size())
//		{
//			outsChanged = true;
//			for (size_t i = outputInfos.size(); i < outputs->size(); ++i)
//			{
//				auto tex = DeserializeTextureInfo(*(*outputs)[i]);
//				nosVulkan->DestroyResource(&tex);
//			}
//		}
//		else
//		{
//			outsChanged = true;
//			for (size_t i = outputs->size(); i < outputInfos.size(); ++i)
//			{
//				auto& output = *outputInfos[i];
//				nosResourceShareInfo tex{};
//				tex.Info.Type = NOS_RESOURCE_TYPE_TEXTURE;
//				tex.Info.Texture = {
//					.Width = output.resolution().x(),
//					.Height = output.resolution().y(),
//					.Format = NOS_FORMAT_R16G16B16A16_UNORM,
//					.Usage = nosImageUsage(NOS_IMAGE_USAGE_SAMPLED | NOS_IMAGE_USAGE_RENDER_TARGET |
//										   NOS_IMAGE_USAGE_TRANSFER_SRC | NOS_IMAGE_USAGE_TRANSFER_DST),
//				};
//				if (nosVulkan->CreateResource(&tex, "LayoutDrawerOut") != NOS_RESULT_SUCCESS)
//					nosEngine.LogE("Failed to create output texture for output %zu!", i);
//				OutTextures.emplace_back(tex);
//			}
//		}
//
//		if (outsChanged)
//		{
//			UpdateOutputTexturesPin();
//			outputs = args.GetPinData<flatbuffers::Vector<flatbuffers::Offset<nos::sys::vulkan::Texture>>>(
//				NSN_OutputTextures);
//		}
//
//		auto& inTexturesFb = *args.GetPinData<flatbuffers::Vector<flatbuffers::Offset<nos::sys::vulkan::Texture>>>(
//			NOS_NAME("InputTextures"));
//		std::vector<nosResourceShareInfo> inTextures;
//		inTextures.reserve(inTexturesFb.size());
//		for (auto* texture : inTexturesFb)
//			inTextures.push_back(DeserializeTextureInfo(*texture));
//
//		bool unmatchedInputTextures = false;
//		for (auto* drawItem : drawItems)
//		{
//			if (drawItem->texture_id() >= inTextures.size())
//			{
//				unmatchedInputTextures = true;
//				break;
//			}
//		}
//		if (unmatchedInputTextures)
//			SetStatusMessage(StatusType::InvalidInputTextures,
//							 "Layout contains draw items referencing invalid input textures",
//							 fb::NodeStatusMessageType::FAILURE);
//		else
//			ClearStatusMessage(StatusType::InvalidInputTextures);
//
//		auto cmd = vkss::BeginCmd(NOS_NAME("LayoutDrawer"), NodeId);
//		for (size_t outIndex = 0; outIndex < outputInfos.size(); outIndex++)
//		{
//			auto const& outInfo = *outputInfos[outIndex];
//			auto outTex = OutTextures[outIndex];
//			if (outTex.Memory.Handle == 0)
//				continue;
//			nosVulkan->Clear(cmd, &outTex, {0.0f, 0.0f, 0.0f, 1.0f});
//
//			auto outSize = outInfo.size();
//			auto outPos = outInfo.pos();
//			std::vector<layout::LayoutDrawItem> drawItemsForOut;
//			drawItemsForOut.reserve(drawItems.size());
//			// Calculate translation and scale matrix of the output texture
//			glm::vec2 translation = {outPos.x(), outPos.y()};
//			glm::vec2 scale = {outSize.x(), outSize.y()};
//			// Apply the transformation to each draw item
//			for (auto* drawItem : drawItems)
//			{
//				auto drawItemPos = drawItem->position();
//				auto drawItemSize = drawItem->size();
//				glm::vec2 pos = {drawItemPos.x(), drawItemPos.y()};
//				glm::vec2 endPos = {drawItemPos.x() + drawItemSize.x(), drawItemPos.y() + drawItemSize.y()};
//
//				glm::vec2 transformedPos = (pos - translation) / scale;
//				glm::vec2 transformedEndPos = (endPos - translation) / scale;
//
//				glm::vec2 newPos = {transformedPos.x, transformedPos.y};
//				glm::vec2 newSize = {transformedEndPos.x - transformedPos.x, transformedEndPos.y - transformedPos.y};
//				layout::LayoutDrawItem newItem{};
//				newItem.mutable_position() = nos::fb::vec2(newPos.x, newPos.y);
//				newItem.mutable_size() = nos::fb::vec2(newSize.x, newSize.y);
//				newItem.mutate_texture_id(drawItem->texture_id());
//				drawItemsForOut.emplace_back(newItem);
//			}
//			DrawOut(cmd, inTextures, drawItemsForOut, outTex);
//		}
//
//		if (*args.GetPinData<bool>(NOS_NAME("PreviewEnabled")))
//		{
//			auto preview = vkss::DeserializeTextureInfo(args[NSN_Preview].Data->Data);
//			if (preview.Memory.Handle != 0)
//			{
//				nosVulkan->Clear(cmd, &preview, {0.0f, 0.0f, 0.0f, 1.0f});
//				std::vector<layout::LayoutDrawItem> drawItemsForPreview;
//				drawItemsForPreview.reserve(drawItems.size());
//				for (auto* drawItem : drawItems)
//					drawItemsForPreview.emplace_back(*drawItem);
//				DrawOut(cmd, inTextures, drawItemsForPreview, preview);
//
//				std::vector<layout::LayoutOutputInfo> outlinesForPreview;
//				outlinesForPreview.reserve(outputInfos.size());
//				for (auto* output : outputInfos)
//					outlinesForPreview.emplace_back(*output);
//				DrawOutlines(cmd, outlinesForPreview, preview);
//			}
//		}
//
//		vkss::EndCmd(cmd, false, nullptr);
//		return NOS_RESULT_SUCCESS;
//	}
//
//	void DrawOut(nosCmd cmd,
//				 std::span<nosResourceShareInfo> textures,
//				 std::span<layout::LayoutDrawItem> drawList,
//				 nosResourceShareInfo const& output)
//	{
//		for (auto const& item : drawList)
//		{
//			if (item.texture_id() >= textures.size())
//				continue;
//			auto const& inputTex = textures[item.texture_id()];
//			auto pos = item.position();
//			auto size = item.size();
//
//			std::array bindings = {vkss::ShaderBinding(NOS_NAME("Offset"), pos),
//								   vkss::ShaderBinding(NOS_NAME("Size"), size),
//								   vkss::ShaderBinding(NOS_NAME("Input"), inputTex)};
//
//			nosVertexData vertexData = {
//				.DepthFunc = NOS_DEPTH_FUNCTION_ALWAYS,
//				.DepthWrite = NOS_FALSE,
//				.DepthTest = NOS_FALSE,
//			};
//			nosRunPassParams runPassParams{.Key = NSN_TexturedQuad_Pass,
//										   .Bindings = bindings.data(),
//										   .BindingCount = bindings.size(),
//										   .Output = output,
//										   .Vertices = vertexData,
//										   .Wireframe = NOS_FALSE,
//										   .Benchmark = NOS_FALSE,
//										   .DoNotClear = true};
//			nosVulkan->RunPass(cmd, &runPassParams);
//		}
//	}
//
//	void DrawOutlines(nosCmd cmd,
//				 std::span<layout::LayoutOutputInfo> outputInfos,
//				 nosResourceShareInfo const& output)
//	{
//		// random color generator, but it should be the same if i call it multiple times
//		std::mt19937_64 rng{}; // seed engine with index
//		std::uniform_real_distribution<> dist(0.0, 1.0);
//
//
//
//		for (auto const& item : outputInfos)
//		{
//			auto pos = item.pos();
//			auto size = item.size();
//			float aspectRatio = output.Info.Texture.Width / (float) output.Info.Texture.Height;
//			float outlineWidth = 20.0f / (float) output.Info.Texture.Width;
//
//			glm::vec4 color = {dist(rng), dist(rng), dist(rng), 1.0f};
//
//			std::array bindings = {
//				vkss::ShaderBinding(NOS_NAME("Offset"), pos),
//				vkss::ShaderBinding(NOS_NAME("Size"), size),
//				vkss::ShaderBinding(NOS_NAME("AspectRatio"), aspectRatio),
//				vkss::ShaderBinding(NOS_NAME("OutlineWidth"), outlineWidth),
//				vkss::ShaderBinding(NOS_NAME("Color"), color),
//			};
//
//			nosVertexData vertexData = {
//				.DepthFunc = NOS_DEPTH_FUNCTION_ALWAYS,
//				.DepthWrite = NOS_FALSE,
//				.DepthTest = NOS_FALSE,
//			};
//			nosRunPassParams runPassParams{.Key = NSN_QuadOutline_Pass,
//										   .Bindings = bindings.data(),
//										   .BindingCount = bindings.size(),
//										   .Output = output,
//										   .Vertices = vertexData,
//										   .Wireframe = NOS_FALSE,
//										   .Benchmark = NOS_FALSE,
//										   .DoNotClear = true};
//			nosVulkan->RunPass(cmd, &runPassParams);
//		}
//	}
};

nosResult RegisterLayoutDrawer(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NSN_LayoutDrawer, LayoutDrawerNode, fn);

	fs::path root = nosEngine.Plugin->RootFolderPath;
	auto fragPath = (root / "Shaders" / "TexturedQuad.frag").generic_string();
	auto vertPath = (root / "Shaders" / "TexturedQuad.vert").generic_string();
	auto outlineFragPath = (root / "Shaders" / "QuadOutline.frag").generic_string();
	auto outlineVertPath = (root / "Shaders" / "QuadOutline.vert").generic_string();

	// Register shaders
	std::array shaders = {
		nosShaderInfo{.ShaderName = NSN_TexturedQuad_Frag,
					  .Source = {.Stage = NOS_SHADER_STAGE_FRAG, .GLSLPath = fragPath.c_str()},
					  .AssociatedNodeClassName = NSN_LayoutDrawer},
		nosShaderInfo{.ShaderName = NSN_TexturedQuad_Vert,
					  .Source = {.Stage = NOS_SHADER_STAGE_VERT, .GLSLPath = vertPath.c_str()},
					  .AssociatedNodeClassName = NSN_LayoutDrawer},
		nosShaderInfo{.ShaderName = NSN_QuadOutline_Frag,
					  .Source = {.Stage = NOS_SHADER_STAGE_FRAG, .GLSLPath = outlineFragPath.c_str()},
					  .AssociatedNodeClassName = NSN_LayoutDrawer},

		nosShaderInfo{.ShaderName = NSN_QuadOutline_Vert,
					  .Source = {.Stage = NOS_SHADER_STAGE_VERT, .GLSLPath = outlineVertPath.c_str()},
					  .AssociatedNodeClassName = NSN_LayoutDrawer},
	};
	auto ret = nosVulkan->RegisterShaders(shaders.size(), shaders.data());
	if (NOS_RESULT_SUCCESS != ret)
		return ret;

	std::array passes = {
		nosPassInfo{
			.Key = NSN_TexturedQuad_Pass,
			.Shader = NSN_TexturedQuad_Frag,
			.VertexShader = NSN_TexturedQuad_Vert,
			.MultiSample = 1,
		},
		nosPassInfo{
			.Key = NSN_QuadOutline_Pass,
			.Shader = NSN_QuadOutline_Frag,
			.VertexShader = NSN_QuadOutline_Vert,
			.MultiSample = 1,
		},
	};

	ret = nosVulkan->RegisterPasses(passes.size(), passes.data());
	return ret;
}
} // namespace nos::utilities
