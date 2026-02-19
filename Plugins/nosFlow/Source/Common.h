#pragma once

#undef GLM_FORCE_SWIZZLE
#undef GLM_SWIZZLE
#include <glm/glm.hpp>
#include <glm/common.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/quaternion_trigonometric.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <glm/matrix.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <numbers>

#include <Nodos/Plugin.hpp>
#include <Nodos/Types.h>

#include <nosSysVulkan/Helpers.hpp>
#include <nosSysVulkan/nosVulkanSubsystem.h>
#include <nosSysVulkan/Types_generated.h>

#include <map>
#include <memory>

namespace nos::flow
{


using nos::Name;
// using nos::PinMapping;
// using nos::InterpretPinValue;
// using nos::GetPinValue;
// using nos::GetPinValues;
using nos::GetPinIds;
using nos::NodeExecuteParams;
using nos::ReadToString;
using nos::MakeUnique;
using nos::MakeShared;

NOS_REGISTER_NAME(Out);
NOS_REGISTER_NAME(In); 
NOS_REGISTER_NAME(Input); 
NOS_REGISTER_NAME(Output); 
NOS_REGISTER_NAME(Texture); 

namespace fb
{
using namespace nos::fb;
}

#define DECLARE_FUNCTION(__class__, __fn__) \
static nosResult __fn__(void* ctx, nosFunctionExecuteParams* params)			\
{																				\
	auto nodeParams = params->ParentNodeExecuteParams;							\
	auto functionParams = params->FunctionNodeExecuteParams;					\
	return static_cast<__class__*>(ctx)->__fn__(nodeParams, functionParams);	\
}

#define DECLARE_GET_FUNCTIONS(_N_) \
inline static std::pair<nos::Name, nosPfnNodeFunctionExecute> Functions[_N_]; \
static nosResult GetFunctions(size_t* outCount, nosName* pName, nosPfnNodeFunctionExecute* outFunction) \
{															\
	*outCount = _N_;										\
	if (!pName || !outFunction)								\
		return NOS_RESULT_SUCCESS;							\
															\
	for (auto& [name, fn] : Functions)						\
	{														\
		*pName++ = name;									\
		*outFunction++ = fn;								\
	}														\
															\
	return NOS_RESULT_SUCCESS;								\
}

template <class T>
inline void RegisterShadersAndPasses()
{
	{
		size_t count;
		T::GetShaders(&count, nullptr);
		std::vector<nosShaderInfo> shaders(count);
		T::GetShaders(&count, shaders.data());
		nosVulkan->RegisterShaders(shaders.size(), shaders.data());
	}

	{
		size_t count;
		T::GetPasses(&count, nullptr);
		std::vector<nosPassInfo> passes(count);
		T::GetPasses(&count, passes.data());
		nosVulkan->RegisterPasses(passes.size(), passes.data());
	}
}


// // update texture size and format if needed
// bool UpdateTextureFormat(nosTextureObject texture, const glm::uvec2& res, const char* tag, nosFormat format = nosFormat::NOS_FORMAT_NONE);
// // resize texture used in output pin
// void UpdateOutputPinTextureFormat(const nos::uuid& pinId, nosResourceInfo& texture, const glm::uvec2& res, const char* tag, nosFormat format = nosFormat::NOS_FORMAT_NONE);

void UpdateVertexBuffer(
	nosVertexData& data, 
	nos::ObjectRef& bufferObject,
	const void* verticesData,
	size_t verticesSize,
	const void* indicesData,
	size_t indicesSize,
	size_t indicesCount,
	const char* tag);


template<typename... Names>
bool HasPinValues(nos::NodeExecuteParams const& params, const Names... paramNames)
{
	return (params.contains(paramNames) && ...);
}

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

enum class EFontAtlas {
	DejaVuSansMono14pt,
	DejaVuSansMono28pt
};

enum class ETextHorizontalAlignment : int {
	Left = 0,
	Middle = 1,
	Right = 2
};
enum class ETextVerticalAlignment : int {
	Top = 0,
	Middle = 1,
	Bottom = 2
};

std::shared_ptr<FontAtlas> MakeFontAtlas(EFontAtlas font, const nos::uuid& nodeId, const char* tag);

struct VertexData : nosVertexData
{
	nos::ObjectRef BufferObject;
};

class TextBuilder {
public:
	static TextBuilder Create(glm::vec2 outputSize, FontAtlas& font);
	TextBuilder& Add(std::string text, float height, glm::vec2 position, ETextHorizontalAlignment halign, ETextVerticalAlignment valign, glm::vec4 color, bool uppercase);
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
		glm::vec4 posAndUv;
		glm::vec4 color;
	};
	glm::vec2 OutputSize;
	FontAtlas& Font;
	std::vector<Vertex> Vertices;
	std::vector<glm::uvec3> Indices;
};


}