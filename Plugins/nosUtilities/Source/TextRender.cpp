// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/PluginHelpers.hpp>
#include <nosVulkanSubsystem/Helpers.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H

namespace nos::utilities
{
namespace fs = std::filesystem;

NOS_REGISTER_NAME(TextRender)
NOS_REGISTER_NAME(TextGlyph_Pass)
NOS_REGISTER_NAME(TextGlyph_Frag)
NOS_REGISTER_NAME(TextGlyph_Vert)
NOS_REGISTER_NAME(TextBox_Pass)
NOS_REGISTER_NAME(TextBox_Frag)
NOS_REGISTER_NAME(TextBox_Vert)

NOS_REGISTER_NAME(Text)
NOS_REGISTER_NAME(FontSize)
NOS_REGISTER_NAME(Color)
NOS_REGISTER_NAME(Opacity)
NOS_REGISTER_NAME(StrokeColor)
NOS_REGISTER_NAME(StrokeWidth)
NOS_REGISTER_NAME(ShadowColor)
NOS_REGISTER_NAME(ShadowOffset)
NOS_REGISTER_NAME(ShadowSoftness)
NOS_REGISTER_NAME(BackgroundColor)
NOS_REGISTER_NAME(BackgroundPadding)
NOS_REGISTER_NAME(HorizontalAlign)
NOS_REGISTER_NAME(VerticalAlign)
NOS_REGISTER_NAME(Position)
NOS_REGISTER_NAME(Resolution)
NOS_REGISTER_NAME(WrapWidth)
NOS_REGISTER_NAME(Font)
NOS_REGISTER_NAME(Output)

NOS_REGISTER_NAME(Offset)
NOS_REGISTER_NAME(Size)
NOS_REGISTER_NAME(AtlasRect)
NOS_REGISTER_NAME(FillColor)
NOS_REGISTER_NAME(Softness)
NOS_REGISTER_NAME(PxRange)
NOS_REGISTER_NAME(Atlas)
NOS_REGISTER_NAME(BoxColor)

// ASCII printable range packed into the SDF atlas.
constexpr uint32_t FIRST_CHAR = 32;
constexpr uint32_t LAST_CHAR = 126;
constexpr uint32_t GLYPH_COUNT = LAST_CHAR - FIRST_CHAR + 1;
// Reference size the atlas is rasterized at; the SDF is scaled by the shader.
constexpr float REF_PIXEL_SIZE = 72.0f;
// SDF spread in reference pixels: how far signed-distance data extends from the
// glyph edge. Caps the usable outline thickness and shadow softness.
constexpr int SDF_SPREAD = 16;
constexpr int ATLAS_WIDTH = 1024;
constexpr int ATLAS_PADDING = 2;
// Upper bound on draw calls per execution to keep pathological inputs cheap.
constexpr size_t MAX_DRAWN_GLYPHS = 8192;

struct Glyph
{
	int AtlasX = 0, AtlasY = 0; // top-left in atlas pixels
	int Width = 0, Height = 0;  // bitmap size in reference pixels
	float BearingLeft = 0;      // pen-origin to bitmap left, reference pixels
	float BearingTop = 0;       // baseline to bitmap top, reference pixels
	float Advance = 0;          // horizontal advance, reference pixels
	bool HasBitmap = false;
};

struct TextRenderNode : NodeContext
{
	FT_Library Library = nullptr;
	nosResourceShareInfo AtlasTex{};

	std::array<Glyph, GLYPH_COUNT> Glyphs{};
	int AtlasW = 0, AtlasH = 0;
	float RefLineHeight = REF_PIXEL_SIZE * 1.2f;
	float RefAscender = REF_PIXEL_SIZE;

	// Path the current atlas was built from; empty means "bundled font".
	std::optional<std::string> BuiltFontPath;
	bool AtlasValid = false;

	TextRenderNode(nosFbNodePtr node) : NodeContext(node)
	{
		if (FT_Init_FreeType(&Library))
		{
			Library = nullptr;
			nosEngine.LogE("TextRender: failed to initialize FreeType");
			return;
		}
		FT_Int spread = SDF_SPREAD;
		FT_Property_Set(Library, "sdf", "spread", &spread);
	}

	~TextRenderNode() override
	{
		DestroyAtlas();
		if (Library)
			FT_Done_FreeType(Library);
	}

	void DestroyAtlas()
	{
		if (AtlasTex.Memory.Handle)
			nosVulkan->DestroyResource(&AtlasTex);
		AtlasTex = {};
		AtlasValid = false;
	}

	std::string ResolveFontPath(const char* fontPin) const
	{
		if (fontPin && fontPin[0] != '\0')
			return fontPin;
		fs::path root = nosEngine.Module->RootFolderPath;
		return (root / "Fonts" / "RobotoMono-Regular.ttf").generic_string();
	}

	// Rasterizes the printable ASCII range into a single-channel SDF atlas
	// and uploads it as a texture. Returns false on failure.
	bool BuildAtlas(const std::string& fontPath)
	{
		if (!Library)
			return false;

		FT_Face face = nullptr;
		if (FT_New_Face(Library, fontPath.c_str(), 0, &face))
		{
			nosEngine.LogE("TextRender: could not open font '%s'", fontPath.c_str());
			return false;
		}
		FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(REF_PIXEL_SIZE));

		RefLineHeight = (face->size->metrics.height >> 6) > 0 ? float(face->size->metrics.height >> 6)
															  : REF_PIXEL_SIZE * 1.2f;
		RefAscender = (face->size->metrics.ascender >> 6) > 0 ? float(face->size->metrics.ascender >> 6)
															  : REF_PIXEL_SIZE;

		struct Raster
		{
			std::vector<uint8_t> Pixels;
			int W = 0, H = 0;
		};
		std::array<Raster, GLYPH_COUNT> rasters{};
		std::array<Glyph, GLYPH_COUNT> glyphs{};

		for (uint32_t i = 0; i < GLYPH_COUNT; ++i)
		{
			Glyph& g = glyphs[i];
			if (FT_Load_Char(face, FIRST_CHAR + i, FT_LOAD_DEFAULT))
				continue;
			FT_GlyphSlot slot = face->glyph;
			g.Advance = float(slot->advance.x >> 6);

			if (FT_Render_Glyph(slot, FT_RENDER_MODE_SDF))
				continue; // whitespace / empty outline: advance is still valid

			const FT_Bitmap& bm = slot->bitmap;
			if (bm.width == 0 || bm.rows == 0)
				continue;

			g.Width = int(bm.width);
			g.Height = int(bm.rows);
			g.BearingLeft = float(slot->bitmap_left);
			g.BearingTop = float(slot->bitmap_top);
			g.HasBitmap = true;

			Raster& r = rasters[i];
			r.W = int(bm.width);
			r.H = int(bm.rows);
			r.Pixels.resize(size_t(r.W) * r.H);
			const int pitch = bm.pitch;
			for (int row = 0; row < r.H; ++row)
			{
				const uint8_t* src = bm.buffer + size_t(row) * (pitch < 0 ? -pitch : pitch);
				std::memcpy(r.Pixels.data() + size_t(row) * r.W, src, r.W);
			}
		}
		FT_Done_Face(face);

		// Shelf-pack the glyph bitmaps into a fixed-width atlas.
		int x = ATLAS_PADDING, y = ATLAS_PADDING, shelfH = 0;
		for (uint32_t i = 0; i < GLYPH_COUNT; ++i)
		{
			Glyph& g = glyphs[i];
			if (!g.HasBitmap)
				continue;
			if (x + g.Width + ATLAS_PADDING > ATLAS_WIDTH)
			{
				x = ATLAS_PADDING;
				y += shelfH + ATLAS_PADDING;
				shelfH = 0;
			}
			g.AtlasX = x;
			g.AtlasY = y;
			x += g.Width + ATLAS_PADDING;
			shelfH = std::max(shelfH, g.Height);
		}
		const int atlasW = ATLAS_WIDTH;
		const int atlasH = y + shelfH + ATLAS_PADDING;

		std::vector<uint8_t> pixels(size_t(atlasW) * atlasH, 0);
		for (uint32_t i = 0; i < GLYPH_COUNT; ++i)
		{
			const Glyph& g = glyphs[i];
			const Raster& r = rasters[i];
			if (!g.HasBitmap)
				continue;
			for (int row = 0; row < r.H; ++row)
				std::memcpy(pixels.data() + size_t(g.AtlasY + row) * atlasW + g.AtlasX,
							r.Pixels.data() + size_t(row) * r.W,
							r.W);
		}

		DestroyAtlas();

		nosResourceShareInfo atlas{};
		atlas.Info.Type = NOS_RESOURCE_TYPE_TEXTURE;
		atlas.Info.Texture = {.Width = uint32_t(atlasW),
							  .Height = uint32_t(atlasH),
							  .Format = NOS_FORMAT_R8_UNORM};
		auto cmd = vkss::BeginCmd(NOS_NAME("TextRenderAtlasUpload"), NodeId);
		nosResult res = nosVulkan->ImageLoad(cmd,
											 pixels.data(),
											 nosVec2u{uint32_t(atlasW), uint32_t(atlasH)},
											 NOS_FORMAT_R8_UNORM,
											 &atlas,
											 "TextRenderAtlas");
		vkss::EndCmd(cmd, NOS_TRUE, nullptr);
		if (res != NOS_RESULT_SUCCESS)
		{
			nosEngine.LogE("TextRender: failed to upload font atlas");
			return false;
		}

		atlas.Info.Texture.Filter = NOS_TEXTURE_FILTER_LINEAR;
		AtlasTex = atlas;
		AtlasW = atlasW;
		AtlasH = atlasH;
		Glyphs = glyphs;
		AtlasValid = true;
		return true;
	}

	void EnsureAtlas(const char* fontPin)
	{
		std::string path = ResolveFontPath(fontPin);
		if (AtlasValid && BuiltFontPath && *BuiltFontPath == path)
			return;
		BuiltFontPath = path;
		if (BuildAtlas(path))
			ClearNodeStatusMessages();
		else
			SetNodeStatusMessage("Could not load font.", fb::NodeStatusMessageType::FAILURE);
	}

	const Glyph* GlyphFor(char c) const
	{
		auto u = uint32_t(uint8_t(c));
		if (u < FIRST_CHAR || u > LAST_CHAR)
			return nullptr;
		return &Glyphs[u - FIRST_CHAR];
	}

	// One laid-out glyph: index into Glyphs plus its pen origin on the line.
	struct Placed
	{
		uint32_t GlyphIndex;
		float PenX;
		int Line;
	};

	// Greedy word-wrap layout in output-pixel space. Honors '\n' and breaks
	// words longer than maxWidth character by character.
	void LayoutText(const char* text,
					float scale,
					float maxWidth,
					std::vector<Placed>& out,
					std::vector<float>& lineWidths) const
	{
		const float spaceAdvance = [&] {
			const Glyph* sp = GlyphFor(' ');
			return sp ? sp->Advance * scale : REF_PIXEL_SIZE * 0.3f * scale;
		}();

		int line = 0;
		float penX = 0.0f;
		lineWidths.push_back(0.0f);

		auto newLine = [&] {
			lineWidths[line] = penX;
			++line;
			penX = 0.0f;
			lineWidths.push_back(0.0f);
		};
		auto placeChar = [&](char c) {
			const Glyph* g = GlyphFor(c);
			if (!g)
				return;
			auto idx = uint32_t(uint8_t(c)) - FIRST_CHAR;
			if (g->HasBitmap && out.size() < MAX_DRAWN_GLYPHS)
				out.push_back({idx, penX, line});
			penX += g->Advance * scale;
		};

		std::string word;
		auto wordWidth = [&](const std::string& w) {
			float width = 0.0f;
			for (char c : w)
				if (const Glyph* g = GlyphFor(c))
					width += g->Advance * scale;
			return width;
		};
		auto flushWord = [&] {
			if (word.empty())
				return;
			float ww = wordWidth(word);
			if (ww > maxWidth)
			{
				// Word does not fit on any line: hard-break per character.
				for (char c : word)
				{
					const Glyph* g = GlyphFor(c);
					float adv = g ? g->Advance * scale : 0.0f;
					if (penX > 0.0f && penX + adv > maxWidth)
						newLine();
					placeChar(c);
				}
			}
			else
			{
				if (penX > 0.0f && penX + ww > maxWidth)
					newLine();
				for (char c : word)
					placeChar(c);
			}
			word.clear();
		};

		for (const char* p = text; *p; ++p)
		{
			char c = *p;
			if (c == '\n')
			{
				flushWord();
				newLine();
			}
			else if (c == ' ' || c == '\t')
			{
				flushWord();
				float adv = (c == '\t') ? spaceAdvance * 4.0f : spaceAdvance;
				if (penX > 0.0f)
					penX += adv;
			}
			else
			{
				word.push_back(c);
			}
		}
		flushWord();
		lineWidths[line] = penX;
	}

	// Draws a flat-colored rectangle (the text-box background).
	void DrawBox(nosCmd cmd,
				 const nosResourceShareInfo& tex,
				 float outW,
				 float outH,
				 float x,
				 float y,
				 float w,
				 float h,
				 nosVec4 boxColor)
	{
		nosVec2 offset{x / outW, y / outH};
		nosVec2 size{w / outW, h / outH};
		std::array bindings = {vkss::ShaderBinding(NSN_Offset, offset),
							   vkss::ShaderBinding(NSN_Size, size),
							   vkss::ShaderBinding(NSN_BoxColor, boxColor)};
		nosVertexData vertexData{
			.DepthFunc = NOS_DEPTH_FUNCTION_ALWAYS,
			.DepthWrite = NOS_FALSE,
			.DepthTest = NOS_FALSE,
		};
		nosRunPassParams pass{.Key = NSN_TextBox_Pass,
							  .Bindings = bindings.data(),
							  .BindingCount = uint32_t(bindings.size()),
							  .Output = tex,
							  .Vertices = vertexData,
							  .Wireframe = NOS_FALSE,
							  .Benchmark = NOS_FALSE,
							  .DoNotClear = NOS_TRUE};
		nosVulkan->RunPass(cmd, &pass);
	}

	// Draws one glyph quad. Used for both the shadow and the fill/stroke pass:
	// the shadow passes the shadow color as the fill with a softened edge.
	void DrawGlyph(nosCmd cmd,
				   const nosResourceShareInfo& tex,
				   float outW,
				   float outH,
				   float glyphLeft,
				   float glyphTop,
				   float glyphW,
				   float glyphH,
				   nosVec4 atlasRect,
				   nosVec4 fillColor,
				   nosVec4 strokeColor,
				   float strokeWidth,
				   float softness,
				   float pxRange)
	{
		nosVec2 offset{glyphLeft / outW, glyphTop / outH};
		nosVec2 size{glyphW / outW, glyphH / outH};
		std::array bindings = {vkss::ShaderBinding(NSN_Offset, offset),
							   vkss::ShaderBinding(NSN_Size, size),
							   vkss::ShaderBinding(NSN_AtlasRect, atlasRect),
							   vkss::ShaderBinding(NSN_FillColor, fillColor),
							   vkss::ShaderBinding(NSN_StrokeColor, strokeColor),
							   vkss::ShaderBinding(NSN_StrokeWidth, strokeWidth),
							   vkss::ShaderBinding(NSN_Softness, softness),
							   vkss::ShaderBinding(NSN_PxRange, pxRange),
							   vkss::ShaderBinding(NSN_Atlas, AtlasTex)};
		nosVertexData vertexData{
			.DepthFunc = NOS_DEPTH_FUNCTION_ALWAYS,
			.DepthWrite = NOS_FALSE,
			.DepthTest = NOS_FALSE,
		};
		nosRunPassParams pass{.Key = NSN_TextGlyph_Pass,
							  .Bindings = bindings.data(),
							  .BindingCount = uint32_t(bindings.size()),
							  .Output = tex,
							  .Vertices = vertexData,
							  .Wireframe = NOS_FALSE,
							  .Benchmark = NOS_FALSE,
							  .DoNotClear = NOS_TRUE};
		nosVulkan->RunPass(cmd, &pass);
	}

	nosResult ExecuteNode(nosNodeExecuteParams* rawParams) override
	{
		NodeExecuteParams args(rawParams);

		const char* fontPin = args.GetPinData<const char>(NSN_Font);
		EnsureAtlas(fontPin);

		auto resolution = *reinterpret_cast<nosVec2u*>(args[NSN_Resolution].Data->Data);
		if (resolution.x == 0 || resolution.y == 0)
			return NOS_RESULT_SUCCESS;

		// Resize the output texture to match the requested resolution.
		auto tex = vkss::DeserializeTextureInfo(args[NSN_Output].Data->Data);
		if (tex.Info.Texture.Width != resolution.x || tex.Info.Texture.Height != resolution.y)
		{
			auto resized = tex;
			resized.Memory = {};
			resized.Info.Texture.Width = resolution.x;
			resized.Info.Texture.Height = resolution.y;
			auto texFb = vkss::ConvertTextureInfo(resized);
			texFb.unscaled = true;
			auto buf = nos::Buffer::From(texFb);
			nosEngine.SetPinValue(args[NSN_Output].Id, {.Data = buf.Data(), .Size = buf.Size()});
			tex = vkss::DeserializeTextureInfo(args[NSN_Output].Data->Data);
		}
		if (tex.Memory.Handle == 0)
			return NOS_RESULT_SUCCESS;

		const char* text = args.GetPinData<const char>(NSN_Text);
		float fontSize = *reinterpret_cast<float*>(args[NSN_FontSize].Data->Data);
		float opacity = std::clamp(*reinterpret_cast<float*>(args[NSN_Opacity].Data->Data), 0.0f, 1.0f);
		auto textColor = *reinterpret_cast<nosVec4*>(args[NSN_Color].Data->Data);
		auto strokeColor = *reinterpret_cast<nosVec4*>(args[NSN_StrokeColor].Data->Data);
		float strokeWidthPin = *reinterpret_cast<float*>(args[NSN_StrokeWidth].Data->Data);
		auto shadowColor = *reinterpret_cast<nosVec4*>(args[NSN_ShadowColor].Data->Data);
		auto shadowOffset = *reinterpret_cast<nosVec2*>(args[NSN_ShadowOffset].Data->Data);
		float shadowSoftnessPin = *reinterpret_cast<float*>(args[NSN_ShadowSoftness].Data->Data);
		auto boxColor = *reinterpret_cast<nosVec4*>(args[NSN_BackgroundColor].Data->Data);
		auto boxPadding = *reinterpret_cast<nosVec2*>(args[NSN_BackgroundPadding].Data->Data);
		auto hAlign = *reinterpret_cast<uint32_t*>(args[NSN_HorizontalAlign].Data->Data);
		auto vAlign = *reinterpret_cast<uint32_t*>(args[NSN_VerticalAlign].Data->Data);
		auto position = *reinterpret_cast<nosVec2*>(args[NSN_Position].Data->Data);
		auto wrapWidthChars = *reinterpret_cast<uint32_t*>(args[NSN_WrapWidth].Data->Data);

		// Global opacity folds into every color's alpha.
		textColor.w *= opacity;
		strokeColor.w *= opacity;
		shadowColor.w *= opacity;
		boxColor.w *= opacity;

		// The frame outside the text box stays transparent.
		auto cmd = vkss::BeginCmd(NOS_NAME("TextRender"), NodeId);
		nosVulkan->Clear(cmd, &tex, nosVec4{0.0f, 0.0f, 0.0f, 0.0f});

		if (AtlasValid && text && text[0] != '\0' && fontSize > 0.0f)
		{
			const float outW = float(tex.Info.Texture.Width);
			const float outH = float(tex.Info.Texture.Height);
			const float scale = fontSize / REF_PIXEL_SIZE;
			const float lineHeight = RefLineHeight * scale;
			const float ascender = RefAscender * scale;
			const float pxRange = 2.0f * float(SDF_SPREAD) * scale;
			// The SDF only carries data within SDF_SPREAD reference pixels of the
			// glyph edge, which bounds outline thickness and shadow softness.
			const float effectLimit = float(SDF_SPREAD) * scale;
			const float strokeWidth = std::clamp(strokeWidthPin, 0.0f, effectLimit);
			const float shadowSoftness = std::clamp(shadowSoftnessPin, 0.0f, effectLimit);

			// WrapWidth is in characters; 0 falls back to the texture width.
			// The character cell width is the font's space advance, which is
			// exact for monospace fonts and approximate for proportional ones.
			float wrapWidth = outW;
			if (wrapWidthChars > 0)
			{
				const Glyph* space = GlyphFor(' ');
				const float charWidth = (space ? space->Advance : REF_PIXEL_SIZE * 0.6f) * scale;
				wrapWidth = float(wrapWidthChars) * charWidth;
			}

			std::vector<Placed> placed;
			std::vector<float> lineWidths;
			LayoutText(text, scale, wrapWidth, placed, lineWidths);

			const uint32_t numLines = uint32_t(lineWidths.size());
			const float blockHeight = lineHeight * float(numLines);

			float vBase = 0.0f;
			if (vAlign == 1) // MIDDLE
				vBase = (outH - blockHeight) * 0.5f;
			else if (vAlign == 2) // BOTTOM
				vBase = outH - blockHeight;
			const float vOffset = vBase + position.y;

			// Per-line horizontal anchor offset (alignment + position nudge).
			std::vector<float> hOffsets(numLines);
			for (uint32_t i = 0; i < numLines; ++i)
			{
				float off = 0.0f;
				if (hAlign == 1) // CENTER
					off = (outW - lineWidths[i]) * 0.5f;
				else if (hAlign == 2) // RIGHT
					off = outW - lineWidths[i];
				hOffsets[i] = off + position.x;
			}

			// Text-block bounds, used for the background box.
			float blockLeft = outW, blockRight = 0.0f;
			bool anyLine = false;
			for (uint32_t i = 0; i < numLines; ++i)
			{
				if (lineWidths[i] <= 0.0f)
					continue;
				anyLine = true;
				blockLeft = std::min(blockLeft, hOffsets[i]);
				blockRight = std::max(blockRight, hOffsets[i] + lineWidths[i]);
			}

			// Background box, behind everything.
			if (anyLine && boxColor.w > 0.0f)
				DrawBox(cmd,
						tex,
						outW,
						outH,
						blockLeft - boxPadding.x,
						vOffset - boxPadding.y,
						(blockRight - blockLeft) + 2.0f * boxPadding.x,
						blockHeight + 2.0f * boxPadding.y,
						boxColor);

			auto glyphRect = [&](const Placed& gp, float& left, float& top, float& w, float& h) {
				const Glyph& g = Glyphs[gp.GlyphIndex];
				const float baseline = vOffset + ascender + float(gp.Line) * lineHeight;
				left = hOffsets[gp.Line] + gp.PenX + g.BearingLeft * scale;
				top = baseline - g.BearingTop * scale;
				w = float(g.Width) * scale;
				h = float(g.Height) * scale;
			};
			auto atlasRectOf = [&](const Placed& gp) {
				const Glyph& g = Glyphs[gp.GlyphIndex];
				return nosVec4{float(g.AtlasX) / float(AtlasW),
							   float(g.AtlasY) / float(AtlasH),
							   float(g.Width) / float(AtlasW),
							   float(g.Height) / float(AtlasH)};
			};

			// Drop shadow: all shadows first so no glyph fill is tinted by a
			// neighbouring glyph's shadow.
			if (shadowColor.w > 0.0f)
			{
				nosVec4 noStroke{0.0f, 0.0f, 0.0f, 0.0f};
				for (const Placed& gp : placed)
				{
					float left, top, w, h;
					glyphRect(gp, left, top, w, h);
					DrawGlyph(cmd,
							  tex,
							  outW,
							  outH,
							  left + shadowOffset.x,
							  top + shadowOffset.y,
							  w,
							  h,
							  atlasRectOf(gp),
							  shadowColor,
							  noStroke,
							  0.0f,
							  shadowSoftness,
							  pxRange);
				}
			}

			// Fill + outline.
			for (const Placed& gp : placed)
			{
				float left, top, w, h;
				glyphRect(gp, left, top, w, h);
				DrawGlyph(cmd,
						  tex,
						  outW,
						  outH,
						  left,
						  top,
						  w,
						  h,
						  atlasRectOf(gp),
						  textColor,
						  strokeColor,
						  strokeWidth,
						  0.0f,
						  pxRange);
			}
		}

		vkss::EndCmd(cmd, NOS_FALSE, nullptr);
		return NOS_RESULT_SUCCESS;
	}
};

static nosResult RegisterShaderPair(const fs::path& root,
									const char* baseName,
									nosName fragKey,
									nosName vertKey)
{
	auto fragPath = (root / "Shaders" / (std::string(baseName) + ".frag")).generic_string();
	auto vertPath = (root / "Shaders" / (std::string(baseName) + ".vert")).generic_string();
	std::array shaders = {
		nosShaderInfo{.ShaderName = fragKey,
					  .Source = {.Stage = NOS_SHADER_STAGE_FRAG, .GLSLPath = fragPath.c_str()},
					  .AssociatedNodeClassName = NSN_TextRender},
		nosShaderInfo{.ShaderName = vertKey,
					  .Source = {.Stage = NOS_SHADER_STAGE_VERT, .GLSLPath = vertPath.c_str()},
					  .AssociatedNodeClassName = NSN_TextRender},
	};
	return nosVulkan->RegisterShaders(shaders.size(), shaders.data());
}

nosResult RegisterTextRender(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NSN_TextRender, TextRenderNode, fn);

	fs::path root = nosEngine.Module->RootFolderPath;
	if (nosResult ret = RegisterShaderPair(root, "TextGlyph", NSN_TextGlyph_Frag, NSN_TextGlyph_Vert);
		ret != NOS_RESULT_SUCCESS)
		return ret;
	if (nosResult ret = RegisterShaderPair(root, "TextBox", NSN_TextBox_Frag, NSN_TextBox_Vert);
		ret != NOS_RESULT_SUCCESS)
		return ret;

	std::array passes = {
		nosPassInfo{
			.Key = NSN_TextGlyph_Pass,
			.Shader = NSN_TextGlyph_Frag,
			.VertexShader = NSN_TextGlyph_Vert,
			.MultiSample = 1,
			.Blend = NOS_BLEND_MODE_ALPHA_BLENDING,
		},
		nosPassInfo{
			.Key = NSN_TextBox_Pass,
			.Shader = NSN_TextBox_Frag,
			.VertexShader = NSN_TextBox_Vert,
			.MultiSample = 1,
			.Blend = NOS_BLEND_MODE_ALPHA_BLENDING,
		},
	};
	return nosVulkan->RegisterPasses(passes.size(), passes.data());
}

} // namespace nos::utilities
