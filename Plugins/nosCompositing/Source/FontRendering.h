#include <Nodos/Plugin.hpp>
#include <nosSysVulkan/Helpers.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace nos::compositing
{

struct FontDescription {
	const unsigned char* Image;
	size_t ImageSize;
	const char32_t* Charset;
	size_t CharsetSize;
	int CellsPerRow;
	int CellWidthPx;
	int CellHeightPx;
	int xCellPaddingPx;
	int yCellPaddingPx;
};

struct FontAtlas {
	FontAtlas(FontDescription &description);

	FontDescription& Description;
	nos::ObjectRef Texture = {};
};

enum class FontType {
	DejaVuSansMono14pt,
	DejaVuSansMono28pt
};

enum class TextHorizontalAlignment : int {
	Left = 0,
	Middle = 1,
	Right = 2
};

enum class TextVerticalAlignment : int {
	Top = 0,
	Middle = 1,
	Bottom = 2
};

std::shared_ptr<FontAtlas> MakeFontAtlas(FontType font, const nos::uuid& nodeId, const char* tag);

struct VertexData : nosVertexData
{
	nos::ObjectRef BufferObject;
};

class TextBuilder {
public:
	static TextBuilder Create(glm::vec2 outputSize, FontAtlas& font);
	TextBuilder& Add(std::string text, float height, glm::vec2 position, TextHorizontalAlignment halign, TextVerticalAlignment valign, glm::vec4 color, bool uppercase);
	VertexData Build(const char* tag);

private:
	TextBuilder(glm::vec2 outputSize, FontAtlas& font)
		: OutputSize(outputSize)
		, Font(font)
	{}
	TextBuilder() = default;
	TextBuilder(const TextBuilder&) = default;
	TextBuilder& operator=(const TextBuilder&) = default;

public:
	TextBuilder(TextBuilder&&) = default;
	TextBuilder& operator=(TextBuilder&&) = default;

private:
	struct Vertex {
		glm::vec4 PosAndUV;
		glm::vec4 Color;
	};
	glm::vec2 OutputSize;
	FontAtlas& Font;
	std::vector<Vertex> Vertices;
	std::vector<glm::uvec3> Indices;
};

}