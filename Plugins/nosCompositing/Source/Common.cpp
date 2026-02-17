#include "Common.h"
#include "DejaVuSansMono.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <cuchar>


namespace nos::compositing
{


static FontDescription DejaVuSansMono14pt = {
	DejaVuSansMono14pt_Image,
	sizeof(DejaVuSansMono14pt_Image),
	DejaVuSansMono_Charset,
	256,
	16,
	32,
	32,
	10,
	5
};
static FontDescription DejaVuSansMono28pt = {
	DejaVuSansMono28pt_Image,
	sizeof(DejaVuSansMono28pt_Image),
	DejaVuSansMono_Charset,
	256,
	16,
	64,
	64,
	20,
	10
};


void UpdateVertexBuffer(
	nosVertexData& data, 
	nos::ObjectRef& bufferObject,
	const void* verticesData,
	size_t verticesSize,
	const void* indicesData,
	size_t indicesSize,
	size_t indicesCount,
	const char* tag)
{
	uint32_t bufferSize = (uint32_t)(verticesSize + indicesSize);

	auto info = nos::sys::vulkan::GetResourceInfo(bufferObject).value_or({});
	if (info.Buffer.Size < bufferSize)
	{
		bufferObject = {};
		info = {};
		info.Type = NOS_RESOURCE_TYPE_BUFFER;
		info.Buffer.Size = bufferSize;
		info.Buffer.Usage = nosBufferUsage(NOS_BUFFER_USAGE_VERTEX_BUFFER | NOS_BUFFER_USAGE_INDEX_BUFFER);
		info.Buffer.MemoryFlags = NOS_MEMORY_FLAGS_HOST_VISIBLE;
		nosVulkan->CreateResource(&info, 0, tag, &bufferObject.GetStorage());
		data.Buffer = bufferObject;
	}

	data.VertexOffset = 0;
	data.IndexOffset = (uint32_t)verticesSize;
	data.IndexCount = (uint32_t)indicesCount;
	
	u8* mapping = nosVulkan->Map(bufferObject);
	if (mapping)
	{
		memcpy(mapping, verticesData, verticesSize);
		memcpy(mapping + verticesSize, indicesData, indicesSize);
	}
}



FontAtlas::FontAtlas(FontDescription& description)
	: Description(description)
	, Texture()
{}

std::shared_ptr<FontAtlas> MakeFontAtlas(EFontAtlas font, const nos::uuid& nodeId, const char* tag)
{
	if (!nosVulkan)
		return nullptr;

	FontDescription *fontDescription = nullptr;
	switch (font) {
	case EFontAtlas::DejaVuSansMono14pt:
		fontDescription = &DejaVuSansMono14pt;
		break;
	case EFontAtlas::DejaVuSansMono28pt:
		fontDescription = &DejaVuSansMono28pt;
		break;
	}
	if (!fontDescription)
		return nullptr;

	auto result = std::make_shared<FontAtlas>(*fontDescription);

	int w = 0;
	int h = 0;
	int d = 0;
	stbi_uc* loadedImage = stbi_load_from_memory(fontDescription->Image, (int)fontDescription->ImageSize, &w, &h, &d, 4);

	nosResourceInfo info = {};

	info.Type = NOS_RESOURCE_TYPE_TEXTURE;
	info.Texture.Width = w;
	info.Texture.Height = h;
	info.Texture.Format = NOS_FORMAT_R16G16B16A16_UNORM;

	result->Texture = {};
	nosVulkan->CreateResource(&info, 0, "Mixer_FontAtlas", &result->Texture.GetStorage());
	nosCmd cmd;
	nosCmdBeginParams bp = {.Name = nos::Name("Font atlas upload"), .AssociatedNodeId = nodeId, .OutCmdHandle = &cmd};
	nosVulkan->Begin(&bp);
	nosVulkan->ImageLoad(cmd, loadedImage, nosVec2u(w, h), NOS_FORMAT_R8G8B8A8_SRGB, result->Texture, NOS_TEXTURE_FILTER_LINEAR);
	nosCmdEndParams endParams{ .ForceSubmit = true };
	nosVulkan->End(cmd, &endParams);

	free(loadedImage);

	return result;
}

TextBuilder TextBuilder::Create(glm::vec2 outputSize, FontAtlas& font) {
	return std::move(TextBuilder(outputSize, font));
}

TextBuilder& TextBuilder::Add(std::string text, float cHeight, glm::vec2 position, ETextHorizontalAlignment halign, ETextVerticalAlignment valign, glm::vec4 color, bool uppercase) {
	float letterHeightPx = float(Font.Description.CellHeightPx - 2 * Font.Description.yCellPaddingPx);
	float fontScale = cHeight / letterHeightPx;
	float outputAspect = OutputSize.x / OutputSize.y;
	float fontAspect = float(Font.Description.CellWidthPx - 2 * Font.Description.xCellPaddingPx) / letterHeightPx;
	float cWidth = cHeight * fontAspect / outputAspect;

    auto fontTextureInfo = nos::sys::vulkan::GetResourceInfo(Font.Texture).value_or({});
	float atlasWidth     = float(fontTextureInfo.Texture.Width);
	float atlasHeight    = float(fontTextureInfo.Texture.Height);
	float xCellUvPadding = float(Font.Description.xCellPaddingPx) / atlasWidth;
	float yCellUvPadding = float(Font.Description.yCellPaddingPx) / atlasHeight;
	float cUvWidth       = float(Font.Description.CellWidthPx) / atlasWidth;
	float cUvHeight = float(Font.Description.CellHeightPx) / atlasHeight;

	std::vector<char32_t> characters;
	std::mbstate_t state{};
	char32_t c32;
	const char* ptr = text.c_str();
	const char* end = text.c_str() + text.size() + 1;
	while (std::size_t rc = std::mbrtoc32(&c32, ptr, end - ptr, &state)) {
		assert(rc != (std::size_t)-3);
		if (rc == (std::size_t)-1)
			break;
		if (rc == (std::size_t)-2)
			break;
		ptr += rc;

		// skip variation selectors
		if (c32 >= 0xfe00 && c32 <= 0xfe0f)
			continue;

		characters.push_back(c32);

	}

	float textWidth = cWidth * characters.size();
	static float offsets[] = { 0.0f, -0.5f, -1.0f };
	float offsetX = offsets[(int)halign] * textWidth;
	float offsetY = offsets[(int)valign] * cHeight;

	for (auto c : characters)
	{
		if (uppercase && c >= 97 && c <= 122) {
			c -= 32;
		}
		int characterIndex = 0; // we use character 0 if no character is found
		for (int i = 0; i < Font.Description.CharsetSize; ++i) {
			if (Font.Description.Charset[i] == c) {
				characterIndex = i;
				break;
			}
		}
		glm::vec2 uv(
			float(characterIndex % Font.Description.CellsPerRow) * cUvWidth,
			float(characterIndex / Font.Description.CellsPerRow) * cUvHeight
		);

		glm::vec2 pos = position;
		pos.x += offsetX;
		pos.y += offsetY;

		size_t firstIndex = Vertices.size();
		Vertices.push_back({ glm::vec4(pos + glm::vec2(0.0f, 0.0f), uv + glm::vec2(xCellUvPadding, yCellUvPadding)), color });
		Vertices.push_back({ glm::vec4(pos + glm::vec2(cWidth, 0.0f), uv + glm::vec2(cUvWidth - xCellUvPadding, yCellUvPadding)), color });
		Vertices.push_back({ glm::vec4(pos + glm::vec2(0.0f, cHeight), uv + glm::vec2(xCellUvPadding, cUvHeight - yCellUvPadding)), color });
		Vertices.push_back({ glm::vec4(pos + glm::vec2(cWidth, cHeight), uv + glm::vec2(cUvWidth - xCellUvPadding, cUvHeight - yCellUvPadding)), color });

		Indices.push_back(glm::uvec3(firstIndex, firstIndex + 2, firstIndex + 1));
		Indices.push_back(glm::uvec3(firstIndex + 2, firstIndex + 3, firstIndex + 1));

		offsetX += cWidth;
	}
	return *this;
}

VertexData TextBuilder::Build(const char* tag)
{
	VertexData verts = {};
	nosResourceInfo bufferInfo = {};
	u32 vsz = (u32)Vertices.size() * sizeof(Vertex);
	u32 isz = (u32)Indices.size() * sizeof(glm::uvec3);
	bufferInfo.Type = NOS_RESOURCE_TYPE_BUFFER;
	bufferInfo.Buffer.Size = vsz + isz;
	bufferInfo.Buffer.Usage = nosBufferUsage(NOS_BUFFER_USAGE_VERTEX_BUFFER | NOS_BUFFER_USAGE_INDEX_BUFFER);
	bufferInfo.Buffer.MemoryFlags = NOS_MEMORY_FLAGS_HOST_VISIBLE;
	verts.VertexOffset = 0;
	verts.IndexOffset = vsz;
	verts.IndexCount = (u32)Indices.size() * 3;

	nosVulkan->CreateResource(&bufferInfo, 0, tag, &verts.BufferObject.GetStorage());
	verts.Buffer = verts.BufferObject;
	u8* mapping = nosVulkan->Map(verts.Buffer);
	memcpy(mapping, Vertices.data(), vsz);
	memcpy(mapping + vsz, Indices.data(), isz);

	return verts;
}


}
