// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/PluginHelpers.hpp>

// External
#include <nosVulkanSubsystem/Helpers.hpp>

#include "Names.h"
#include "glm/glm.hpp"

namespace nos::utilities
{
NOS_REGISTER_NAME(TextureGrid);
NOS_REGISTER_NAME(TextureGrid_Frag);
NOS_REGISTER_NAME(TextureGrid_Vert);
NOS_REGISTER_NAME(TextureGrid_Pass);
struct TextureGridNode : nos::NodeContext
{
	enum class GridTexturePinType : uint32_t
	{
		Texture,
		Offset,
		Size,
		KeepAspectRatio,
		Count = KeepAspectRatio + 1,
		None
	};
	struct GridTexture
	{
		std::array<uuid, size_t(GridTexturePinType::Count)> Ids;
	};
	std::map<uint32_t, GridTexture> GridTextures;

	std::optional<std::pair<GridTexturePinType, uint32_t>> GetTypeAndIndexFromStr(std::string const& str)
	{
		std::string typeStr = str.substr(0, str.find_last_of("_"));
		GridTexturePinType type = GridTexturePinType::None;
		if (typeStr == "Input")
			type = GridTexturePinType::Texture;
		else if (typeStr == "Offset")
			type = GridTexturePinType::Offset;
		else if (typeStr == "Size")
			type = GridTexturePinType::Size;
		else if (typeStr == "KeepAspectRatio")
			type = GridTexturePinType::KeepAspectRatio;
		else
			return std::nullopt;

		std::string indexStr = str.substr(str.find_last_of("_") + 1);
		if (indexStr.empty())
			return std::nullopt;
		uint32_t index = 0;
		if (std::from_chars(indexStr.data(), indexStr.data() + indexStr.size(), index).ec != std::errc())
			return std::nullopt;
		return std::make_pair(type, index);
	}

	nosResult OnCreate(nosFbNodePtr node) override
	{
		std::map<uint32_t, GridTexture> tempGridTextures;
		nos::TPartialNodeUpdate partialUpdate{};
		partialUpdate.node_id = NodeId;
		for (auto const& [_, pin] : Pins)
		{
			if (pin.ShowAs != nos::fb::ShowAs::OUTPUT_PIN)
			{
				if (auto typeAndIndex = GetTypeAndIndexFromStr(pin.Name.AsString()))
				{
					auto& writeId = tempGridTextures[typeAndIndex->second].Ids[static_cast<size_t>(typeAndIndex->first)];
					if (writeId)
					{
						writeId = id;
						continue;
					}
				}
				partialUpdate.pins_to_delete.push_back(id);
			}
		}
		for (auto& [_, gridTexture] : tempGridTextures)
		{
			bool allValid = true;
			for (uint32_t i = 0; i < size_t(GridTexturePinType::Count); ++i)
			{
				if (!gridTexture.Ids[i].is_nil())
					continue;
				allValid = false;
				break;
			}
			for (uint32_t i = 0; i < size_t(GridTexturePinType::Count); ++i)
			{
				if (allValid)
				{
					auto& pinUpdate =
						partialUpdate.pins_to_update.emplace_back(std::make_unique<nos::TPartialPinUpdate>());
					pinUpdate->pin_id = gridTexture.Ids[i];
					pinUpdate->orphan_state = std::make_unique<nos::fb::TPinOrphanState>();
					pinUpdate->orphan_state->type = nos::fb::PinOrphanStateType::ACTIVE;
				}
				else
				{
					partialUpdate.pins_to_delete.push_back(gridTexture.Ids[i]);
				}
			}
		}
		GridTextures = std::move(tempGridTextures);
		flatbuffers::FlatBufferBuilder fbb;
		HandleEvent(CreateAppEvent(fbb, CreatePartialNodeUpdate(fbb, &partialUpdate)));
		return NOS_RESULT_SUCCESS;
	}

	nosResult ExecuteNode(nosNodeExecuteParams* params) override
	{
		nos::NodeExecuteParams pins(params);
		auto outputTex = pins.GetPinData<vkss::TexturePinData>(NSN_Output);
		auto outputSize = *pins.GetPinData<nos::fb::vec2i>(NOS_NAME("OutputSize"));
		if (outputTex.Info.Texture.Width != outputSize.x() || outputTex.Info.Texture.Height != outputSize.y())
		{
			outputTex.Memory = {};
			outputTex.Info.Texture.Width = outputSize.x();
			outputTex.Info.Texture.Height = outputSize.y();
			auto tTex = vkss::ConvertTextureInfo(outputTex);
			tTex.unscaled = true;
			SetPinValue(NSN_Output, tTex);
			outputTex = pins.GetPinData<vkss::TexturePinData>(NSN_Output);
		}
		bool first = true;
		for (auto const& [_, gridTexture] : GridTextures)
		{
			std::array<nos::Name, size_t(GridTexturePinType::Count)> pinNames{};
			bool allFound = true;
			for (uint32_t i = 0; i < size_t(GridTexturePinType::Count); ++i)
			{
				if (auto pinName = GetPinName(gridTexture.Ids[i]))
				{
					pinNames[i] = *pinName;
					if (pins.contains(*pinName))
						continue;
				}
				allFound = false;
				break;
			}
			if (!allFound)
				continue;
			auto inputTex = pins.GetPinData<vkss::TexturePinData>(pinNames[size_t(GridTexturePinType::Texture)]);
			auto offset = *pins.GetPinData<nos::fb::vec2i>(pinNames[size_t(GridTexturePinType::Offset)]);
			auto size = *pins.GetPinData<nos::fb::vec2i>(pinNames[size_t(GridTexturePinType::Size)]);
			auto keepAspectRatio = *pins.GetPinData<bool>(pinNames[size_t(GridTexturePinType::KeepAspectRatio)]);

			glm::vec2 offsetValue = {offset.x() / (float)outputTex.Info.Texture.Width,
									 offset.y() / (float)outputTex.Info.Texture.Height};

			glm::vec2 sizeValue = {size.x() / (float)outputTex.Info.Texture.Width,
								   size.y() / (float)outputTex.Info.Texture.Height};

			if (keepAspectRatio)
				sizeValue.y = size.x() * ((inputTex.Info.Texture.Height / (float)inputTex.Info.Texture.Width) /
										  (float)outputTex.Info.Texture.Height);

			std::array bindings = {vkss::ShaderBinding(NOS_NAME("Offset"), offsetValue),
								   vkss::ShaderBinding(NOS_NAME("Size"), sizeValue),
								   vkss::ShaderBinding(NOS_NAME("Input"), inputTex)};

			nosVertexData vertexData = {
				.DepthFunc = NOS_DEPTH_FUNCTION_ALWAYS,
				.DepthWrite = NOS_FALSE,
				.DepthTest = NOS_FALSE,
			};
			nosRunPassParams runPassParams{
				.Key = NSN_TextureGrid_Pass,
				.Bindings = bindings.data(),
				.BindingCount = bindings.size(),
				.Output = outputTex,
				.Vertices = vertexData,
				.Wireframe = NOS_FALSE,
				.Benchmark = NOS_FALSE,
				.DoNotClear = !first,
				.ClearCol = {0.0f, 0.0f, 0.0f, 1.0f},
				.CullMode = NOS_CULL_MODE_NONE
			};
			auto cmd = vkss::BeginCmd(NOS_NAME("TextureGrid"), NodeId);
			nosVulkan->RunPass(cmd, &runPassParams);
			vkss::EndCmd(cmd, false, nullptr);
			first = false;
		}
		return NOS_RESULT_SUCCESS;
	}

	void OnMenuRequested(nosContextMenuRequestPtr request) override
	{
		flatbuffers::FlatBufferBuilder fbb;

		std::vector<flatbuffers::Offset<nos::ContextMenuItem>> items;
		if (*request->item_id() == NodeId)
			items.push_back(nos::CreateContextMenuItemDirect(fbb, "Add Input", 1));
		else
		{
			auto& pin = *GetPin(*request->item_id());
			if (pin.ShowAs == nosFbShowAs::OUTPUT_PIN)
				return;
			items.push_back(nos::CreateContextMenuItemDirect(fbb, "Remove Input", 1));
		}

		auto event = CreateAppEvent(
			fbb,
			CreateAppContextMenuUpdate(
				fbb, request->item_id(), request->pos(), request->instigator(), fbb.CreateVector(items)));

		HandleEvent(event);
	}

	void OnMenuCommand(uuid const& itemID, uint32_t cmd) override
	{
		flatbuffers::FlatBufferBuilder fbb;
		nos::TPartialNodeUpdate update{};
		update.node_id = NodeId;
		if (itemID == NodeId)
		{
			uint32_t firstNonPresentPinIndex = 0;
			for (; firstNonPresentPinIndex < GridTextures.size(); ++firstNonPresentPinIndex)
			{
				if (!GridTextures.contains(firstNonPresentPinIndex))
					break;
			}
			auto& newGridTexture = GridTextures[firstNonPresentPinIndex];
			for (uint32_t i = 0; i < size_t(GridTexturePinType::Count); ++i)
			{
				newGridTexture.Ids[i] = nosEngine.GenerateID();
				auto& pin = update.pins_to_add.emplace_back(std::make_unique<nos::fb::TPin>());
				pin->can_show_as = nos::fb::CanShowAs::INPUT_PIN_OR_PROPERTY;
				pin->id = newGridTexture.Ids[i];
				
				switch (GridTexturePinType(i))
				{
				case GridTexturePinType::Texture:
					pin->name = "Input_" + std::to_string(firstNonPresentPinIndex);
					pin->display_name = "Input " + std::to_string(firstNonPresentPinIndex + 1);
					pin->type_name = nos::sys::vulkan::Texture::GetFullyQualifiedName();
					pin->show_as = nos::fb::ShowAs::INPUT_PIN;
					break;
				case GridTexturePinType::Offset:
					pin->name = "Offset_" + std::to_string(firstNonPresentPinIndex);
					pin->display_name = "Offset " + std::to_string(firstNonPresentPinIndex + 1);
					pin->type_name = nos::fb::vec2i::GetFullyQualifiedName();
					pin->show_as = nos::fb::ShowAs::PROPERTY;
					break;
				case GridTexturePinType::Size:
					pin->name = "Size_" + std::to_string(firstNonPresentPinIndex);
					pin->display_name = "Size " + std::to_string(firstNonPresentPinIndex+1);
					pin->type_name = nos::fb::vec2i::GetFullyQualifiedName();
					pin->show_as = nos::fb::ShowAs::PROPERTY;
					pin->data = nos::PackPinData<nos::fb::vec2i>(nos::fb::vec2i{1920, 1080});
					break;
				case GridTexturePinType::KeepAspectRatio:
					pin->name = "KeepAspectRatio_" + std::to_string(firstNonPresentPinIndex);
					pin->display_name = "Keep Aspect Ratio " + std::to_string(firstNonPresentPinIndex + 1);
					pin->type_name = "bool";
					pin->show_as = nos::fb::ShowAs::PROPERTY;
					pin->data = nos::PackPinData<bool>(true);
					break;
				}
			}	
		}
		else
		{
			auto& pin = *GetPin(itemID);
			if (pin.ShowAs == nosFbShowAs::OUTPUT_PIN)
				return;
			auto typeAndIndex = GetTypeAndIndexFromStr(pin.Name.AsString());
			if (!typeAndIndex)
				return;
			auto it = GridTextures.find(typeAndIndex->second);
			if (it == GridTextures.end())
				return;
			auto& gridTexture = it->second;
			// Delete all pins of this grid texture
			for (uint32_t i = 0; i < size_t(GridTexturePinType::Count); ++i)
				update.pins_to_delete.emplace_back(gridTexture.Ids[i]);
			GridTextures.erase(it);
		}
		HandleEvent(CreateAppEvent(fbb, CreatePartialNodeUpdate(fbb, &update)));
	}

};

nosResult RegisterTextureGrid(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NSN_TextureGrid, TextureGridNode, fn);

	fs::path root = nosEngine.Plugin->RootFolderPath;
	auto fragPath = (root / "Shaders" / "TextureGrid.frag").generic_string();
	auto vertPath = (root / "Shaders" / "TextureGrid.vert").generic_string();

	// Register shaders
	std::array shaders = {
		nosShaderInfo{.ShaderName = NSN_TextureGrid_Frag,
								 .Source = {.Stage = NOS_SHADER_STAGE_FRAG, .GLSLPath = fragPath.c_str()},
								 .AssociatedNodeClassName = NSN_TextureGrid},
		nosShaderInfo{.ShaderName = NSN_TextureGrid_Vert,
								 .Source = {.Stage = NOS_SHADER_STAGE_VERT, .GLSLPath = vertPath.c_str()},
								 .AssociatedNodeClassName = NSN_TextureGrid}};
	auto ret = nosVulkan->RegisterShaders(shaders.size(), shaders.data());
	if (NOS_RESULT_SUCCESS != ret)
		return ret;

	nosPassInfo pass = {
		.Key = NSN_TextureGrid_Pass,
		.Shader = NSN_TextureGrid_Frag,
		.VertexShader = NSN_TextureGrid_Vert,
		.MultiSample = 1,
	};

	ret = nosVulkan->RegisterPasses(1, &pass);
	return ret;

	return NOS_RESULT_SUCCESS;
}

}
