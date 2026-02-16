// Copyright ZeroDensity AS. All Rights Reserved.
#include "TextureTransitionUtils.h"

static std::string NosFormatStr(nosFormat format) {
	switch (format) {
	case NOS_FORMAT_NONE:
		return "NONE";
	case NOS_FORMAT_R8_UNORM:
		return "R8_UNORM";
	case NOS_FORMAT_R8_UINT:
		return "R8_UINT";
	case NOS_FORMAT_R8_SRGB:
		return "R8_SRGB";
	case NOS_FORMAT_R8G8_UNORM:
		return "R8G8_UNORM";
	case NOS_FORMAT_R8G8_UINT:
		return "R8G8_UINT";
	case NOS_FORMAT_R8G8_SRGB:
		return "R8G8_SRGB";
	case NOS_FORMAT_R8G8B8_UNORM:
		return "R8G8B8_UNORM";
	case NOS_FORMAT_R8G8B8_SRGB:
		return "R8G8B8_SRGB";
	case NOS_FORMAT_B8G8R8_UNORM:
		return "B8G8R8_UNORM";
	case NOS_FORMAT_B8G8R8_UINT:
		return "B8G8R8_UINT";
	case NOS_FORMAT_B8G8R8_SRGB:
		return "B8G8R8_SRGB";
	case NOS_FORMAT_R8G8B8A8_UNORM:
		return "R8G8B8A8_UNORM";
	case NOS_FORMAT_R8G8B8A8_UINT:
		return "R8G8B8A8_UINT";
	case NOS_FORMAT_R8G8B8A8_SRGB:
		return "R8G8B8A8_SRGB";
	case NOS_FORMAT_B8G8R8A8_UNORM:
		return "B8G8R8A8_UNORM";
	case NOS_FORMAT_B8G8R8A8_SRGB:
		return "B8G8R8A8_SRGB";
	case NOS_FORMAT_A2R10G10B10_UNORM_PACK32:
		return "A2R10G10B10_UNORM_PACK32";
	case NOS_FORMAT_A2R10G10B10_SNORM_PACK32:
		return "A2R10G10B10_SNORM_PACK32";
	case NOS_FORMAT_A2R10G10B10_USCALED_PACK32:
		return "A2R10G10B10_USCALED_PACK32";
	case NOS_FORMAT_A2R10G10B10_SSCALED_PACK32:
		return "A2R10G10B10_SSCALED_PACK32";
	case NOS_FORMAT_A2R10G10B10_UINT_PACK32:
		return "A2R10G10B10_UINT_PACK32";
	case NOS_FORMAT_A2R10G10B10_SINT_PACK32:
		return "A2R10G10B10_SINT_PACK32";
	case NOS_FORMAT_R16_UNORM:
		return "R16_UNORM";
	case NOS_FORMAT_R16_SNORM:
		return "R16_SNORM";
	case NOS_FORMAT_R16_USCALED:
		return "R16_USCALED";
	case NOS_FORMAT_R16_SSCALED:
		return "R16_SSCALED";
	case NOS_FORMAT_R16_UINT:
		return "R16_UINT";
	case NOS_FORMAT_R16_SINT:
		return "R16_SINT";
	case NOS_FORMAT_R16_SFLOAT:
		return "R16_SFLOAT";
	case NOS_FORMAT_R16G16_UNORM:
		return "R16G16_UNORM";
	case NOS_FORMAT_R16G16_SNORM:
		return "R16G16_SNORM";
	case NOS_FORMAT_R16G16_USCALED:
		return "R16G16_USCALED";
	case NOS_FORMAT_R16G16_SSCALED:
		return "R16G16_SSCALED";
	case NOS_FORMAT_R16G16_UINT:
		return "R16G16_UINT";
	case NOS_FORMAT_R16G16_SINT:
		return "R16G16_SINT";
	case NOS_FORMAT_R16G16_SFLOAT:
		return "R16G16_SFLOAT";
	case NOS_FORMAT_R16G16B16_UNORM:
		return "R16G16B16_UNORM";
	case NOS_FORMAT_R16G16B16_SNORM:
		return "R16G16B16_SNORM";
	case NOS_FORMAT_R16G16B16_USCALED:
		return "R16G16B16_USCALED";
	case NOS_FORMAT_R16G16B16_SSCALED:
		return "R16G16B16_SSCALED";
	case NOS_FORMAT_R16G16B16_UINT:
		return "R16G16B16_UINT";
	case NOS_FORMAT_R16G16B16_SINT:
		return "R16G16B16_SINT";
	case NOS_FORMAT_R16G16B16_SFLOAT:
		return "R16G16B16_SFLOAT";
	case NOS_FORMAT_R16G16B16A16_UNORM:
		return "R16G16B16A16_UNORM";
	case NOS_FORMAT_R16G16B16A16_SNORM:
		return "R16G16B16A16_SNORM";
	case NOS_FORMAT_R16G16B16A16_USCALED:
		return "R16G16B16A16_USCALED";
	case NOS_FORMAT_R16G16B16A16_SSCALED:
		return "R16G16B16A16_SSCALED";
	case NOS_FORMAT_R16G16B16A16_UINT:
		return "R16G16B16A16_UINT";
	case NOS_FORMAT_R16G16B16A16_SINT:
		return "R16G16B16A16_SINT";
	case NOS_FORMAT_R16G16B16A16_SFLOAT:
		return "R16G16B16A16_SFLOAT";
	case NOS_FORMAT_R32_UINT:
		return "R32_UINT";
	case NOS_FORMAT_R32_SINT:
		return "R32_SINT";
	case NOS_FORMAT_R32_SFLOAT:
		return "R32_SFLOAT";
	case NOS_FORMAT_R32G32_UINT:
		return "R32G32_UINT";
	case NOS_FORMAT_R32G32_SINT:
		return "R32G32_SINT";
	case NOS_FORMAT_R32G32_SFLOAT:
		return "R32G32_SFLOAT";
	case NOS_FORMAT_R32G32B32_UINT:
		return "R32G32B32_UINT";
	case NOS_FORMAT_R32G32B32_SINT:
		return "R32G32B32_SINT";
	case NOS_FORMAT_R32G32B32_SFLOAT:
		return "R32G32B32_SFLOAT";
	case NOS_FORMAT_R32G32B32A32_UINT:
		return "R32G32B32A32_UINT";
	case NOS_FORMAT_R32G32B32A32_SINT:
		return "R32G32B32A32_SINT";
	case NOS_FORMAT_R32G32B32A32_SFLOAT:
		return "R32G32B32A32_SFLOAT";
	case NOS_FORMAT_B10G11R11_UFLOAT_PACK32:
		return "B10G11R11_UFLOAT_PACK32";
	case NOS_FORMAT_D16_UNORM:
		return "D16_UNORM";
	case NOS_FORMAT_X8_D24_UNORM_PACK32:
		return "X8_D24_UNORM_PACK32";
	case NOS_FORMAT_D32_SFLOAT:
		return "D32_SFLOAT";
	case NOS_FORMAT_G8B8G8R8_422_UNORM:
		return "G8B8G8R8_422_UNORM";
	case NOS_FORMAT_B8G8R8G8_422_UNORM:
		return "B8G8R8G8_422_UNORM";
	default:
		return "Invalid";
	}
}

static std::string NosFieldTypeStr(nosTextureFieldType fieldType) {
	switch (fieldType) {
	case NOS_TEXTURE_FIELD_TYPE_UNKNOWN:
		return "Unknown";
	case NOS_TEXTURE_FIELD_TYPE_EVEN:
	case NOS_TEXTURE_FIELD_TYPE_ODD:
		return "Interlace";
	case NOS_TEXTURE_FIELD_TYPE_PROGRESSIVE:
		return "Progressive";
	default:
		return "Invalid";
	}
}

namespace nos
{


	enum class EZDTransitionType
	{
		Cut,
		Dissolve,
		WipeLeft,
		WipeRight,
		WipeUp,
		WipeDown
	};

	enum class EZDTransitionTarget
	{
		Program,
		Preview
	};

	static std::shared_ptr<FontAtlas> fontAtlas14pt = nullptr;
	static std::shared_ptr<FontAtlas> fontAtlas28pt = nullptr;

	// Pins and parameters
	NOS_REGISTER_NAME(Channel1); // nos.fb.Texture node input pin
	NOS_REGISTER_NAME(Channel2); // nos.fb.Texture node input pin
	NOS_REGISTER_NAME(Channel3); // nos.fb.Texture node input pin
	NOS_REGISTER_NAME(Channel4); // nos.fb.Texture node input pin
	NOS_REGISTER_NAME(Channel5); // nos.fb.Texture node input pin
	NOS_REGISTER_NAME(Channel6); // nos.fb.Texture node input pin
	NOS_REGISTER_NAME(Channel7); // nos.fb.Texture node input pin
	NOS_REGISTER_NAME(Channel8); // nos.fb.Texture node input pin
	NOS_REGISTER_NAME(Channel9); // nos.fb.Texture node input pin
	NOS_REGISTER_NAME(Channel10); // nos.fb.Texture node input pin
	NOS_REGISTER_NAME(Overlay); // nos.fb.Texture node input pin

	// Multiviewer
	NOS_REGISTER_NAME(EnableDebug); // string node parameter

	// Channels
	NOS_REGISTER_NAME(ProgramChannel); // nos.utilities.Channel node input or property pin
	NOS_REGISTER_NAME(PreviewChannel); // nos.utilities.Channel node input or property pin

	// State
	NOS_REGISTER_NAME(IsInTransition);
	NOS_REGISTER_NAME(Amount); // float node readonly parameter + float shader parameter

	// Overlay options
	NOS_REGISTER_NAME(Premultiply); // float node input pin
	NOS_REGISTER_NAME(Opacity); // float shader parameter + node input pin
	NOS_REGISTER_NAME(OverlayPreview);

	// Functions and their arguments
	NOS_REGISTER_NAME(DoTransition); // function name
	NOS_REGISTER_NAME(Duration); // DoTransition function float parameter
	NOS_REGISTER_NAME(Type); // DoTransition function enum parameter
	NOS_REGISTER_NAME(WipeWidth); // DoTransition function float parameter
	NOS_REGISTER_NAME(Target); // DoTransition function enum parameter
	NOS_REGISTER_NAME(Interpolation); // DoTransition function enum parameter
	NOS_REGISTER_NAME(EaseExponent); // DoTransition function float parameter
	NOS_REGISTER_NAME(StepCount); // DoTransition function uint parameter

	// Outputs
	NOS_REGISTER_NAME(Program); // nos.fb.Texture node output pin
	NOS_REGISTER_NAME(Preview); // nos.fb.Texture node output pin
	NOS_REGISTER_NAME(Multiviewer); // nos.fb.Texture node output pin

	// shader and pass
	NOS_REGISTER_NAME(MIXER_OVERLAY_PASS);
	NOS_REGISTER_NAME(MIXER_MULTIVIEWER_PASS);
	NOS_REGISTER_NAME(MIXER_LABEL_PASS);
	NOS_REGISTER_NAME(MIXER_DEBUG_TEXT_PASS);
	NOS_REGISTER_NAME(TRANSITION_DISSOLVE_PASS);
	NOS_REGISTER_NAME(TRANSITION_WIPE_PASS);

	// Shader paraemeters
	NOS_REGISTER_NAME(InBase); // texture shader parameter
	NOS_REGISTER_NAME(InOverlay); // texture shader parameter
	NOS_REGISTER_NAME(UV); // vec2 shader parameter
	NOS_REGISTER_NAME(BlendingMode); // int shader parameter

	NOS_REGISTER_NAME(Font)
	NOS_REGISTER_NAME(MultiviewerLabels);

	NOS_REGISTER_NAME(Input1); // texture shader parameter
	NOS_REGISTER_NAME(Input2); // texture shader parameter
	NOS_REGISTER_NAME(UseX); // int shader parameter
	NOS_REGISTER_NAME(Reverse); // int shader parameter
	NOS_REGISTER_NAME(HalfWidth); // float shader parameter

	NOS_REGISTER_NAME(Channels); // nos.fb.Texture node input pin
	NOS_REGISTER_NAME(TexelSize); // vec2 shader parameter

	struct MixerContext : public NodeContext
	{
		bool bIsOverlayConnected = false;

		// Transition state
		nos::ObjectRef TransitionTexture = {};
		EZDTransitionInterpolation TransitionInterp = EZDTransitionInterpolation::EaseInOut;
		float TransitionEaseExponent = 2.0f;
		u32 TransitionStepCount = 5;
		EZDTransitionType TransitionType = EZDTransitionType::Dissolve;
		float WipeWidth = 0.4f;
		float TransitionDuration = 1.0f;
		EZDTransitionTarget TransitionTarget = EZDTransitionTarget::Program;
		bool bIsInTransition = false;
		std::chrono::steady_clock::time_point TransitionStart;
		u32 TransitionProgramChannel;
		u32 TransitionPreviewChannel;

		//
		bool NeedsLabelsUpdate = true;
		
		nos::ObjectRef MultiviewerLabelsTexture = {};

		//
		struct TextureDebugInfo {
			uint32_t Width;
			uint32_t Height;
			nosTextureFieldType FieldType;
			nosFormat Format;
		};
		bool bDebugEnabled = false;
		TextureDebugInfo LastChannelDebugInfo[10];
		TextureDebugInfo LastOverlayDebugInfo;
		int LastProgramDebugChannelIndex = -1;
		TextureDebugInfo LastProgramDebugInfo;
		nosVertexData ProgramDebugText = {};
		nos::ObjectRef ProgramDebugTextBuffer = {};
		TextureDebugInfo LastPreviewDebugInfo;
		int LastPreviewDebugChannelIndex = -1;
		nosVertexData PreviewDebugText = {};
		nos::ObjectRef PreviewDebugTextBuffer = {};
		TextureDebugInfo LastMultiviewerDebugInfo;
		nosVertexData MultiviewerDebugText = {};
		nos::ObjectRef MultiviewerDebugTextBuffer = {};


		nosResult OnCreate(nosFbNodePtr node) override
		{
			if (!fontAtlas14pt) {
				fontAtlas14pt = MakeFontAtlas(EFontAtlas::DejaVuSansMono14pt, NodeId, "Mixer_fontAtlas14pt");
			}
			if (!fontAtlas28pt) {
				fontAtlas28pt = MakeFontAtlas(EFontAtlas::DejaVuSansMono28pt, NodeId, "Mixer_fontAtlas28pt");
			}
			return NOS_RESULT_SUCCESS;
		}


		virtual ~MixerContext() {
			//if (TransitionTexture.Memory.Handle) {
			//	nosVulkan->DestroyResource(&TransitionTexture);
			//}
			////
			//if (MultiviewerLabelsTexture.Memory.Handle) {
			//	nosVulkan->DestroyResource(&MultiviewerLabelsTexture);
			//}
			////
			//if (ProgramDebugText.Buffer.Memory.Handle) {
			//	nosVulkan->DestroyResource(&ProgramDebugText.Buffer);
			//}
			//if (PreviewDebugText.Buffer.Memory.Handle) {
			//	nosVulkan->DestroyResource(&PreviewDebugText.Buffer);
			//}
			//if (MultiviewerDebugText.Buffer.Memory.Handle) {
			//	nosVulkan->DestroyResource(&MultiviewerDebugText.Buffer);
			//}
		}

		virtual void OnPinUpdated(const nosPinUpdate* update) override {
			if (update->UpdatedField == NOS_PIN_FIELD_DISPLAY_NAME) {
				if (
					update->PinName == NSN_Channel1 ||
					update->PinName == NSN_Channel2 ||
					update->PinName == NSN_Channel3 ||
					update->PinName == NSN_Channel4 ||
					update->PinName == NSN_Channel5 ||
					update->PinName == NSN_Channel6 ||
					update->PinName == NSN_Channel7 ||
					update->PinName == NSN_Channel8 ||
					update->PinName == NSN_Channel9 ||
					update->PinName == NSN_Channel10
				) {
					NeedsLabelsUpdate = true;
					nosEngine.SetNodeDirty(NodeId);
				}
			}
		}

		virtual void OnPinValueChanged(nos::Name pinName, const nos::uuid& pinId, nosBuffer value) override {
			if (pinName == NSN_EnableDebug) {
				bDebugEnabled = *(bool*)value.Data;
			}
		}

		virtual void OnPinConnected(nos::Name pinName, const nos::uuid& connectedPin) override
		{
			if (pinName == NSN_Overlay) {
				bIsOverlayConnected = true;
			}
		}

		virtual void OnPinDisconnected(nos::Name pinName) override
		{
			if (pinName == NSN_Overlay) {
				bIsOverlayConnected = false;
			}
		}


		static nosResult GetFunctions(size_t* outCount, nosName* pName, nosPfnNodeFunctionExecute* outFunction)
		{
			static std::pair<nos::Name, nosPfnNodeFunctionExecute> Functions[1] = {
				{NOS_NAME_STATIC("DoTransition"), MixerContext::DoTransition}
			};

			*outCount = sizeof(Functions) / sizeof(Functions[0]);

			if (!pName || !outFunction)
				return NOS_RESULT_SUCCESS;

			for (auto& [name, fn] : Functions)
			{
				*pName++ = name;
				*outFunction++ = fn;
			}

			return NOS_RESULT_SUCCESS;
		}


		static nos::ObjectRef GetChannelTexture(nos::NodeExecuteParams const& params, u32 index) {
			static const nos::Name channelNames[] = {
				NSN_Channel1,
				NSN_Channel2,
				NSN_Channel3,
				NSN_Channel4,
				NSN_Channel5,
				NSN_Channel6,
				NSN_Channel7,
				NSN_Channel8,
				NSN_Channel9,
				NSN_Channel10
			};
			return params.GetPinObject(channelNames[index]);
		}


		std::string GetChannelName(u32 index) {
			static const nos::Name channels[] = {
				NSN_Channel1,
				NSN_Channel2,
				NSN_Channel3,
				NSN_Channel4,
				NSN_Channel5,
				NSN_Channel6,
				NSN_Channel7,
				NSN_Channel8,
				NSN_Channel9,
				NSN_Channel10
			};
			return Pins[PinName2Id[channels[index]]].DisplayName.AsString();
		}

		void Calculate_ChannelLabels(nos::NodeExecuteParams const& params, nosCmd& cmd)
		{
			if (!MultiviewerLabelsTexture)
				return;

			auto MultiviewerLabelsTextureInfo = nos::sys::vulkan::GetResourceInfo(MultiviewerLabelsTexture).value_or({});
			glm::vec2 outputSize(
				MultiviewerLabelsTextureInfo.Texture.Width,
				MultiviewerLabelsTextureInfo.Texture.Height
			);

			const glm::vec4 white(1.0f, 1.0f, 1.0f, 1.0f);
			auto verts = TextBuilder::Create(outputSize, *fontAtlas28pt)
				.Add("Program", 0.033333f, glm::vec2(0.25f, 0.516666f), ETextHorizontalAlignment::Middle, ETextVerticalAlignment::Middle, white, true)
				.Add("Preview", 0.033333f, glm::vec2(0.75f, 0.516666f), ETextHorizontalAlignment::Middle, ETextVerticalAlignment::Middle, white, true)
				.Add(Pins[PinName2Id[NSN_Channel1]].DisplayName.AsCStr(), 0.033333f, glm::vec2(0.1f, 0.749666f), ETextHorizontalAlignment::Middle, ETextVerticalAlignment::Middle, white, true)
				.Add(Pins[PinName2Id[NSN_Channel2]].DisplayName.AsCStr(), 0.033333f, glm::vec2(0.3f, 0.749666f), ETextHorizontalAlignment::Middle, ETextVerticalAlignment::Middle, white, true)
				.Add(Pins[PinName2Id[NSN_Channel3]].DisplayName.AsCStr(), 0.033333f, glm::vec2(0.5f, 0.749666f), ETextHorizontalAlignment::Middle, ETextVerticalAlignment::Middle, white, true)
				.Add(Pins[PinName2Id[NSN_Channel4]].DisplayName.AsCStr(), 0.033333f, glm::vec2(0.7f, 0.749666f), ETextHorizontalAlignment::Middle, ETextVerticalAlignment::Middle, white, true)
				.Add(Pins[PinName2Id[NSN_Channel5]].DisplayName.AsCStr(), 0.033333f, glm::vec2(0.9f, 0.749666f), ETextHorizontalAlignment::Middle, ETextVerticalAlignment::Middle, white, true)
				.Add(Pins[PinName2Id[NSN_Channel6]].DisplayName.AsCStr(), 0.033333f, glm::vec2(0.1f, 0.983333f), ETextHorizontalAlignment::Middle, ETextVerticalAlignment::Middle, white, true)
				.Add(Pins[PinName2Id[NSN_Channel7]].DisplayName.AsCStr(), 0.033333f, glm::vec2(0.3f, 0.983333f), ETextHorizontalAlignment::Middle, ETextVerticalAlignment::Middle, white, true)
				.Add(Pins[PinName2Id[NSN_Channel8]].DisplayName.AsCStr(), 0.033333f, glm::vec2(0.5f, 0.983333f), ETextHorizontalAlignment::Middle, ETextVerticalAlignment::Middle, white, true)
				.Add(Pins[PinName2Id[NSN_Channel9]].DisplayName.AsCStr(), 0.033333f, glm::vec2(0.7f, 0.983333f), ETextHorizontalAlignment::Middle, ETextVerticalAlignment::Middle, white, true)
				.Add(Pins[PinName2Id[NSN_Channel10]].DisplayName.AsCStr(), 0.033333f, glm::vec2(0.9f, 0.983333f), ETextHorizontalAlignment::Middle, ETextVerticalAlignment::Middle, white, true)
				.Build("MixerNode_Calculate_ChannelLabels");

			nosRunPassParams labelPassParams = {};
			labelPassParams.Key = NSN_MIXER_LABEL_PASS;
			std::vector bindings = {
			  nos::sys::vulkan::ShaderTextureBinding(NSN_Font, fontAtlas28pt->Texture, NOS_TEXTURE_FILTER_LINEAR),
			};
			labelPassParams.Vertices = verts;
			labelPassParams.Bindings = bindings.data();
			labelPassParams.BindingCount = (u32)bindings.size();
			labelPassParams.Output = MultiviewerLabelsTexture;
			nosVulkan->RunPass(cmd, &labelPassParams);
			// nosVulkan->DestroyResource(&verts.Buffer);
		}


		void AddTextureDebugText(TextBuilder& textBuilder, const char* label, TextureDebugInfo info, glm::vec2 pos, float fontSize)
		{
			std::string debugText[] = {
				label,
				std::format("{} x {}", info.Width, info.Height),
				NosFieldTypeStr(info.FieldType),
				NosFormatStr(info.Format)
			};
			constexpr glm::vec4 yellow(1.0f, 1.0f, 0.0f, 1.0f);
			constexpr int lineCount = sizeof(debugText) / sizeof(debugText[0]);
			for (int lineI = 0; lineI < lineCount; ++lineI) {
				textBuilder.Add(
					debugText[lineI].c_str(),
					fontSize,
					pos + glm::vec2(0.0f, (lineI - lineCount) * fontSize),
					ETextHorizontalAlignment::Left,
					ETextVerticalAlignment::Top,
					yellow,
					false
				);
			}
		}


		bool HasTextureDebugInfoChanged(TextureDebugInfo& lastInfo, nos::ObjectRef& currentTexture) const 
		{
			nosTextureFieldType currentFieldType = {};
			nosVulkan->GetResourceFieldType(currentTexture, &currentFieldType);
			auto currentInfo = nos::sys::vulkan::GetResourceInfo(currentTexture).value_or({});
			bool isInterlace = currentFieldType== NOS_TEXTURE_FIELD_TYPE_ODD || currentFieldType== NOS_TEXTURE_FIELD_TYPE_EVEN;
			bool isLastInterlace = lastInfo.FieldType == NOS_TEXTURE_FIELD_TYPE_ODD || lastInfo.FieldType == NOS_TEXTURE_FIELD_TYPE_EVEN;
			bool changed =
				lastInfo.Width != currentInfo.Texture.Width ||
				lastInfo.Height != currentInfo.Texture.Height ||
				lastInfo.Format != currentInfo.Texture.Format ||
				isInterlace != isLastInterlace && lastInfo.FieldType != currentFieldType;

			if (changed)
			{
				lastInfo.Width  = currentInfo.Texture.Width;
				lastInfo.Height = currentInfo.Texture.Height;
				lastInfo.Format = currentInfo.Texture.Format;
				lastInfo.FieldType = currentFieldType;
			}
			return changed;
		}

		void UpdateDebugTextsIfNeeded(nos::NodeExecuteParams const& params)
		{
			u32 programChannelIndex = *params.GetPinData<u32>(NSN_ProgramChannel);
			bool programChannelIndexChanged = programChannelIndex != LastProgramDebugChannelIndex;
			LastProgramDebugChannelIndex = programChannelIndex;
			u32 previewChannelIndex = *params.GetPinData<u32>(NSN_PreviewChannel);
			bool previewChannelIndexChanged = previewChannelIndex != LastPreviewDebugChannelIndex;
			LastPreviewDebugChannelIndex = previewChannelIndex;

			bool programChannelChanged = false;
			bool previewChannelChanged = false;

			//
			nos::ObjectRef multiviewerTexture = params.GetPinObject(NSN_Multiviewer);
			auto multiviewerTextureInfo = nos::sys::vulkan::GetResourceInfo(multiviewerTexture).value_or({});


			nos::ObjectRef channels[10] = {
				params.GetPinObject(NSN_Channel1),
				params.GetPinObject(NSN_Channel2),
				params.GetPinObject(NSN_Channel3),
				params.GetPinObject(NSN_Channel4),
				params.GetPinObject(NSN_Channel5),
				params.GetPinObject(NSN_Channel6),
				params.GetPinObject(NSN_Channel7),
				params.GetPinObject(NSN_Channel8),
				params.GetPinObject(NSN_Channel9),
				params.GetPinObject(NSN_Channel10)
			};

			bool needsMultiviewerDebugTextUpdate = HasTextureDebugInfoChanged(LastMultiviewerDebugInfo, multiviewerTexture);

			for (int i = 0; i < 10; ++i) {
				if (HasTextureDebugInfoChanged(LastChannelDebugInfo[i], channels[i])) {
					needsMultiviewerDebugTextUpdate = true;

					if (i == programChannelIndex)
						programChannelChanged = true;
					if (i == previewChannelIndex)
						previewChannelChanged = true;
				}
			}
			if (needsMultiviewerDebugTextUpdate) {
				MultiviewerDebugTextBuffer = {};
				glm::vec2 outputSize(multiviewerTextureInfo.Texture.Width, multiviewerTextureInfo.Texture.Height);
				auto textBuilder = TextBuilder::Create(outputSize, *fontAtlas14pt);
				for (int i = 0; i < 10; ++i) {
					glm::vec2 pos(
						0.2f * (i % 5),
						0.733333f + 0.23333f * (i / 5)
					);
					AddTextureDebugText(textBuilder, "", LastChannelDebugInfo[i], pos, 0.018f);
				}
				AddTextureDebugText(textBuilder, "Multiviewer", LastMultiviewerDebugInfo, glm::vec2(0.0f, 4*0.018f), 0.018f);
				MultiviewerDebugText = textBuilder.Build("MultiviewerDebugText");
			}

			//
			nos::ObjectRef programTexture = params.GetPinObject(NSN_Program);
			auto programTextureInfo = nos::sys::vulkan::GetResourceInfo(programTexture).value_or({});

			bool needsProgramDebugTextUpdate = programChannelChanged || programChannelIndexChanged || NeedsLabelsUpdate;
			if (HasTextureDebugInfoChanged(LastProgramDebugInfo, programTexture)) {
				needsProgramDebugTextUpdate = true;
			}

			if (needsProgramDebugTextUpdate) {
				ProgramDebugText = {};

				glm::vec2 outputSize(programTextureInfo.Texture.Width, programTextureInfo.Texture.Height);
				auto textBuilder = TextBuilder::Create(outputSize, *fontAtlas14pt);
				AddTextureDebugText(
					textBuilder,
					GetChannelName(programChannelIndex).c_str(),
					LastChannelDebugInfo[programChannelIndex],
					glm::vec2(0.0f, 1.0f - 5 * 0.036f),
					0.036f
				);
				AddTextureDebugText(
					textBuilder,
					"Program",
					LastProgramDebugInfo,
					glm::vec2(0.0f, 1.0f),
					0.036f
				);
				//textBuilder.AddBackground(0.005f, 0.995f, 0.3f, 0.995f - 9 * 0.036f, glm::vec4(0.0f, 0.0f, 0.0f, 0.85f));
				ProgramDebugText = textBuilder.Build("MixerNode_ProgramDebugText");
			}

			//
			nos::ObjectRef previewTexture = params.GetPinObject(NSN_Preview);
			auto previewTextureInfo = nos::sys::vulkan::GetResourceInfo(previewTexture).value_or({});

			bool needsPreviewDebugTextUpdate = previewChannelChanged || previewChannelIndexChanged || NeedsLabelsUpdate;
			if (HasTextureDebugInfoChanged(LastPreviewDebugInfo, previewTexture)) {
				needsPreviewDebugTextUpdate = true;
			}
			if (needsPreviewDebugTextUpdate) {
				PreviewDebugTextBuffer = {};
				glm::vec2 outputSize(previewTextureInfo.Texture.Width, previewTextureInfo.Texture.Height);
				auto textBuilder = TextBuilder::Create(outputSize, *fontAtlas14pt);
				AddTextureDebugText(
					textBuilder,
					GetChannelName(previewChannelIndex).c_str(),
					LastChannelDebugInfo[previewChannelIndex],
					glm::vec2(0.0f, 1.0f - 5 * 0.036f),
					0.036f
				);
				AddTextureDebugText(
					textBuilder,
					"Preview",
					LastPreviewDebugInfo,
					glm::vec2(0.0f, 1.0f),
					0.036f
				);
				PreviewDebugText = textBuilder.Build("MixerNode_PreviewDebugText");
			}
		}


		void UpdateTransition(float elapsed)
		{
			float amount = glm::clamp(elapsed / TransitionDuration, 0.0f, 1.0f);
			amount = GetInterpolation(TransitionInterp, amount, TransitionStepCount, TransitionEaseExponent);
			amount = glm::clamp(amount, 0.0f, 1.0f);
			if (amount >= 1.0f)
			{
				if (TransitionTarget == EZDTransitionTarget::Program)
				{
					nosEngine.SetPinValueByName(NodeId, NSN_ProgramChannel, { &TransitionPreviewChannel, sizeof(u32) });
					nosEngine.SetPinValueByName(NodeId, NSN_PreviewChannel, { &TransitionProgramChannel, sizeof(u32) });
				}
				amount = 0.0f;
				bIsInTransition = false;
				nosEngine.SetPinValueByName(NodeId, NSN_IsInTransition, { &bIsInTransition, sizeof(bIsInTransition) });
			}
			nosEngine.SetPinValueByName(NodeId, NSN_Amount, { &amount, sizeof(amount) });
		}


		nos::ObjectRef getTransitionTextureInfo(nos::NodeExecuteParams const& params)
		{
			auto outputTextureInfo = nos::sys::vulkan::GetResourceInfo(params.GetPinObject(NSN_Program)).value_or({});
			auto TransitionTextureInfo = nos::sys::vulkan::GetResourceInfo(TransitionTexture).value_or({});

			if (TransitionTextureInfo.Texture.Width == outputTextureInfo.Texture.Width
				&& TransitionTextureInfo.Texture.Height == outputTextureInfo.Texture.Height
				&& TransitionTextureInfo.Texture.Format == outputTextureInfo.Texture.Format) {
				return TransitionTexture;
			}

			TransitionTexture = {};

			TransitionTextureInfo.Type = NOS_RESOURCE_TYPE_TEXTURE;
			TransitionTextureInfo.Texture.Width = outputTextureInfo.Texture.Width;
			TransitionTextureInfo.Texture.Height = outputTextureInfo.Texture.Height;
			TransitionTextureInfo.Texture.Format = outputTextureInfo.Texture.Format;
			nosVulkan->CreateResource(&TransitionTextureInfo, 0, "MixerNode_TransitionTexture", &TransitionTexture.GetStorage());

			return TransitionTexture;
		}


		bool UpdateLabelsTextureIfNeeded(nos::NodeExecuteParams const& params)
		{
			auto outputTextureInfo = nos::sys::vulkan::GetResourceInfo(params.GetPinObject(NSN_Program)).value_or({});
			auto MultiviewerLabelsTextureInfo = nos::sys::vulkan::GetResourceInfo(MultiviewerLabelsTexture).value_or({});
			if (!MultiviewerLabelsTexture
				|| MultiviewerLabelsTextureInfo.Texture.Width != outputTextureInfo.Texture.Width
				|| MultiviewerLabelsTextureInfo.Texture.Height != outputTextureInfo.Texture.Height
				|| MultiviewerLabelsTextureInfo.Texture.Format != outputTextureInfo.Texture.Format
			) {
				MultiviewerLabelsTexture = {};
				nosVulkan->CreateResource(&outputTextureInfo, 0, "MixerNode_MultiviewerLabelsTexture", &MultiviewerLabelsTexture.GetStorage());
				return true;
			}
			else {
				return false;
			}
		}


		void Calculate_Program(nos::NodeExecuteParams const& params, nosCmd& cmd)
		{
			bool isInTransition = *params.GetPinData<bool>(NSN_IsInTransition);
			bool NeedsTransition = isInTransition && TransitionTarget == EZDTransitionTarget::Program;
			if (NeedsTransition)
			{
			    Calculate_Transition(params, cmd);
			}

			u32 programChannelIndex = *params.GetPinData<u32>(NSN_ProgramChannel);
			auto baseTexture = NeedsTransition
				? getTransitionTextureInfo(params)
				: GetChannelTexture(params, programChannelIndex);
			auto overlayTexture = params.GetPinObject(NSN_Overlay);
			auto outputTexture = params.GetPinObject(NSN_Program);
			auto OverlayOpacity = bIsOverlayConnected
				? *params.GetPinData<float>(NSN_Opacity)
				: 0.0f;
			auto overlayUV = glm::vec4(0.0, 0.0, 1.0, 1.0);
			auto blendingMode = *params.GetPinData<bool>(NSN_Premultiply) ? 1 : 0;

			nosRunPassParams programPassParams = {};
			programPassParams.Key = NSN_MIXER_OVERLAY_PASS;
			std::vector bindings = {
			  nos::sys::vulkan::ShaderTextureBinding(NSN_InBase,    baseTexture, NOS_TEXTURE_FILTER_LINEAR),
			  nos::sys::vulkan::ShaderTextureBinding(NSN_InOverlay, overlayTexture, NOS_TEXTURE_FILTER_LINEAR),
			  nos::sys::vulkan::ShaderDataBinding(NSN_UV, overlayUV),
			  nos::sys::vulkan::ShaderDataBinding(NSN_Opacity, OverlayOpacity),
			  nos::sys::vulkan::ShaderDataBinding(NSN_BlendingMode, blendingMode),
			};
			programPassParams.Bindings = bindings.data();
			programPassParams.BindingCount = (u32)bindings.size();
			programPassParams.Output = outputTexture;
			nosVulkan->RunPass(cmd, &programPassParams);

			if (bDebugEnabled) {
				nosRunPassParams debugTextPassParams = {};
				debugTextPassParams.Key = NSN_MIXER_DEBUG_TEXT_PASS;
				std::vector bindings = {
					nos::sys::vulkan::ShaderTextureBinding(NSN_Font, fontAtlas14pt->Texture, NOS_TEXTURE_FILTER_LINEAR),
				};
				debugTextPassParams.DoNotClear = true;
				debugTextPassParams.Vertices = ProgramDebugText;
				debugTextPassParams.Bindings = bindings.data();
				debugTextPassParams.BindingCount = (u32)bindings.size();
				debugTextPassParams.Output = outputTexture;
				nosVulkan->RunPass(cmd, &debugTextPassParams);
			}
		}


		void Calculate_Preview(nos::NodeExecuteParams const& params, nosCmd& cmd)
		{
			bool isInTransition = *params.GetPinData<bool>(NSN_IsInTransition);
			bool NeedsTransition = isInTransition && TransitionTarget == EZDTransitionTarget::Preview;
			if (NeedsTransition)
			{
			    Calculate_Transition(params, cmd);
			}

			u32 previewChannelIndex = *params.GetPinData<u32>(NSN_PreviewChannel);
			auto baseTexture = NeedsTransition
				? getTransitionTextureInfo(params)
				: GetChannelTexture(params, previewChannelIndex);
			auto overlayTexture= params.GetPinObject(NSN_Overlay);
			auto outputTexture = params.GetPinObject(NSN_Preview);
			auto OverlayOpacity = (bIsOverlayConnected && *params.GetPinData<bool>(NSN_OverlayPreview))
				? *params.GetPinData<float>(NSN_Opacity)
				: 0.0f;
			auto overlayUV = glm::vec4(0.0, 0.0, 1.0, 1.0);
			auto blendingMode = *params.GetPinData<bool>(NSN_Premultiply) ? 1 : 0;

			nosRunPassParams previewPassParams = {};
			previewPassParams.Key = NSN_MIXER_OVERLAY_PASS;
			std::vector bindings = {
			  nos::sys::vulkan::ShaderTextureBinding(NSN_InBase, baseTexture, NOS_TEXTURE_FILTER_LINEAR),
			  nos::sys::vulkan::ShaderTextureBinding(NSN_InOverlay, overlayTexture, NOS_TEXTURE_FILTER_LINEAR),
			  nos::sys::vulkan::ShaderDataBinding(NSN_UV, overlayUV),
			  nos::sys::vulkan::ShaderDataBinding(NSN_Opacity, OverlayOpacity),
			  nos::sys::vulkan::ShaderDataBinding(NSN_BlendingMode, blendingMode),
			};
			previewPassParams.Bindings = bindings.data();
			previewPassParams.BindingCount = (u32)bindings.size();
			previewPassParams.Output = outputTexture;
			nosVulkan->RunPass(cmd, &previewPassParams);

			if (bDebugEnabled) {
				nosRunPassParams debugTextPassParams = {};
				debugTextPassParams.Key = NSN_MIXER_DEBUG_TEXT_PASS;
				std::vector bindings = {
					nos::sys::vulkan::ShaderTextureBinding(NSN_Font, fontAtlas14pt->Texture, NOS_TEXTURE_FILTER_LINEAR),
				};
				debugTextPassParams.DoNotClear = true;
				debugTextPassParams.Vertices = PreviewDebugText;
				debugTextPassParams.Bindings = bindings.data();
				debugTextPassParams.BindingCount = (u32)bindings.size();
				debugTextPassParams.Output = outputTexture;
				nosVulkan->RunPass(cmd, &debugTextPassParams);
			}
		}


		void Calculate_Multiviewer(nos::NodeExecuteParams const& params, nosCmd& cmd)
		{
			bool updatedLabelsTexture = UpdateLabelsTextureIfNeeded(params);
			if (NeedsLabelsUpdate || updatedLabelsTexture) {
				Calculate_ChannelLabels(params, cmd);
				NeedsLabelsUpdate = false;
			}

			auto outputTexture = params.GetPinObject(NSN_Multiviewer);

			auto outputTextureInfo = nos::sys::vulkan::GetResourceInfo(outputTexture).value_or({});

			nos::fb::vec2 texelSize(
				1.0f / outputTextureInfo.Texture.Width,
				1.0f / outputTextureInfo.Texture.Height
			);
			u32 programChannelIndex = *params.GetPinData<u32>(NSN_ProgramChannel);
			u32 previewChannelIndex = *params.GetPinData<u32>(NSN_PreviewChannel);
			auto programTexture = params.GetPinObject(NSN_Program);
			auto previewTexture = params.GetPinObject(NSN_Preview);
			
			nosTextureObject channels[10] = {
				params.GetPinObject(NSN_Channel1),
				params.GetPinObject(NSN_Channel2),
				params.GetPinObject(NSN_Channel3),
				params.GetPinObject(NSN_Channel4),
				params.GetPinObject(NSN_Channel5),
				params.GetPinObject(NSN_Channel6),
				params.GetPinObject(NSN_Channel7),
				params.GetPinObject(NSN_Channel8),
				params.GetPinObject(NSN_Channel9),
				params.GetPinObject(NSN_Channel10)
			};

			std::vector<nosTextureFilter> filters(10, NOS_TEXTURE_FILTER_LINEAR);

			{
				nosRunPassParams multiviewerPassParams = {};
				multiviewerPassParams.Key = NSN_MIXER_MULTIVIEWER_PASS;
				std::vector bindings = {
					nos::sys::vulkan::ShaderTextureBinding(NSN_Program, programTexture, NOS_TEXTURE_FILTER_LINEAR),
					nos::sys::vulkan::ShaderTextureBinding(NSN_Preview, previewTexture, NOS_TEXTURE_FILTER_LINEAR),
					nos::sys::vulkan::ShaderTextureBinding(NSN_MultiviewerLabels, MultiviewerLabelsTexture, NOS_TEXTURE_FILTER_LINEAR),
					nos::sys::vulkan::ShaderDataBinding(NSN_ProgramChannel, programChannelIndex),
					nos::sys::vulkan::ShaderDataBinding(NSN_PreviewChannel, previewChannelIndex),
					nos::sys::vulkan::ShaderDataBinding(NSN_TexelSize, texelSize),
					nos::sys::vulkan::ShaderTextureArrayBinding(NSN_Channels, channels, filters.data(), 10),
				};
				multiviewerPassParams.Bindings = bindings.data();
				multiviewerPassParams.BindingCount = (u32)bindings.size();
				multiviewerPassParams.Output = outputTexture;
				nosVulkan->RunPass(cmd, &multiviewerPassParams);
			}

			if (bDebugEnabled) {
				nosRunPassParams debugTextPassParams = {};
				debugTextPassParams.Key = NSN_MIXER_DEBUG_TEXT_PASS;
				std::vector bindings = {
					nos::sys::vulkan::ShaderTextureBinding(NSN_Font, fontAtlas14pt->Texture, NOS_TEXTURE_FILTER_LINEAR),
				};
				debugTextPassParams.DoNotClear = true;
				debugTextPassParams.Vertices = MultiviewerDebugText;
				debugTextPassParams.Bindings = bindings.data();
				debugTextPassParams.BindingCount = (u32)bindings.size();
				debugTextPassParams.Output = outputTexture;
				nosVulkan->RunPass(cmd, &debugTextPassParams);
			}
		}


		void Calculate_Transition(nos::NodeExecuteParams const& params, nosCmd& cmd)
		{
			float amount = *params.GetPinData<float>(NSN_Amount);;
			auto programTexture = GetChannelTexture(params, TransitionProgramChannel);
			auto previewTexture = GetChannelTexture(params, TransitionPreviewChannel);

			auto& source1Texture = programTexture;
			auto& source2Texture = previewTexture;
			auto& sourceTextureInfo = TransitionTarget == EZDTransitionTarget::Program
				? programTexture
				: previewTexture;
			auto outputTextureInfo = getTransitionTextureInfo(params);
			if (TransitionType == EZDTransitionType::Dissolve)
			{
				nosRunPassParams transitionPassParams = {};
				transitionPassParams.Key = NSN_TRANSITION_DISSOLVE_PASS;
				std::vector bindings = {
					nos::sys::vulkan::ShaderTextureBinding(NSN_Input1, source1Texture, NOS_TEXTURE_FILTER_LINEAR),
					nos::sys::vulkan::ShaderTextureBinding(NSN_Input2, source2Texture, NOS_TEXTURE_FILTER_LINEAR),
					nos::sys::vulkan::ShaderDataBinding(NSN_Amount, amount),
				};
				transitionPassParams.Bindings = bindings.data();
				transitionPassParams.BindingCount = (u32)bindings.size();
				transitionPassParams.Output = outputTextureInfo;
				nosVulkan->RunPass(cmd, &transitionPassParams);
			}
			else
			{
				auto wipeLeft = TransitionType == EZDTransitionType::WipeLeft;
				auto wipeRight = TransitionType == EZDTransitionType::WipeRight;
				auto wipeUp = TransitionType == EZDTransitionType::WipeUp;
				auto wipeDown = TransitionType == EZDTransitionType::WipeDown;

				auto useX = (wipeLeft || wipeRight) ? 1 : 0;
				auto reverse = (wipeLeft || wipeUp) ? 1 : 0;
				auto wipeHalfWidth = 0.5f * WipeWidth;

				if (wipeLeft || wipeRight || wipeUp || wipeDown)
				{
					nosRunPassParams transitionPassParams = {};
					transitionPassParams.Key = NSN_TRANSITION_WIPE_PASS;
					std::vector bindings = {
						nos::sys::vulkan::ShaderTextureBinding(NSN_Input1, source1Texture, NOS_TEXTURE_FILTER_LINEAR),
						nos::sys::vulkan::ShaderTextureBinding(NSN_Input2, source2Texture, NOS_TEXTURE_FILTER_LINEAR),
						nos::sys::vulkan::ShaderDataBinding(NSN_Amount, amount),
						nos::sys::vulkan::ShaderDataBinding(NSN_UseX, useX),
						nos::sys::vulkan::ShaderDataBinding(NSN_Reverse, reverse),
						nos::sys::vulkan::ShaderDataBinding(NSN_HalfWidth, wipeHalfWidth),
					};
					transitionPassParams.Bindings = bindings.data();
					transitionPassParams.BindingCount = (u32)bindings.size();
					transitionPassParams.Output = outputTextureInfo;
					nosVulkan->RunPass(cmd, &transitionPassParams);
				}
			}
		}

		nosResult ExecuteNode(nos::NodeExecuteParams const& params)
		{
			//if (!HasPinValues(
			//	params,
			//	NSN_Channel1,
			//	NSN_Channel2,
			//	NSN_Channel3,
			//	NSN_Channel4,
			//	NSN_Channel5,
			//	NSN_Channel6,
			//	NSN_Channel7,
			//	NSN_Channel8,
			//	NSN_Channel9,
			//	NSN_Channel10,
			//	NSN_Overlay,
			//	NSN_ProgramChannel,
			//	NSN_PreviewChannel,
			//	NSN_OverlayPreview,
			//	NSN_Opacity,
			//	NSN_Premultiply,
			//	NSN_Program,
			//	NSN_Preview,
			//	NSN_Multiviewer
			//)) {
			//	return NOS_RESULT_INVALID_ARGUMENT;
			//}

			if (bIsInTransition) {
				auto now = std::chrono::high_resolution_clock::now();
				auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - TransitionStart);
				UpdateTransition(elapsed.count() / 1000000.0f);
			}

			if (bDebugEnabled) {
				UpdateDebugTextsIfNeeded(params);
			}

			nosCmd cmd;
			nosCmdBeginParams bp = {.Name = nos::Name("Run"), .AssociatedNodeId = NodeId, .OutCmdHandle = &cmd};
			nosVulkan->Begin(&bp);

			Calculate_Program(params, cmd);
			Calculate_Preview(params, cmd);
			Calculate_Multiviewer(params, cmd);

			nosVulkan->End(cmd, 0);

			return NOS_RESULT_SUCCESS;
		}


		nosResult CopyFrom(nosCopyFromInfo* copyInfo)
		{
			if (bIsInTransition)
				nosEngine.SetPinDirty(*GetPinId(NSN_Amount));
			return NOS_RESULT_SUCCESS;
		}


		static nosResult DoTransition(void* ctx, nosFunctionExecuteParams* params)
		{
			auto nodeExecParams = params->ParentNodeExecuteParams;
			auto functionExecParams = params->FunctionNodeExecuteParams;
			if (!HasPinValues(nodeExecParams, NSN_ProgramChannel, NSN_PreviewChannel) ||
				!HasPinValues(functionExecParams, NSN_Duration, NSN_Type, NSN_WipeWidth, NSN_Target, NSN_Interpolation, NSN_EaseExponent, NSN_StepCount)) {
				// TODO - it should return error instead of failing silently
				return NOS_RESULT_FAILED;
			}

			auto context = (MixerContext*)ctx;

			auto nodeParams = nos::NodeExecuteParams(nodeExecParams);
			if (context->bIsInTransition)
				return NOS_RESULT_SUCCESS;

			u32 programChannelIndex = *nodeParams.GetPinData<u32>(NSN_ProgramChannel);
			u32 previewChannelIndex = *nodeParams.GetPinData<u32>(NSN_PreviewChannel);
			if (programChannelIndex == previewChannelIndex)
				return NOS_RESULT_SUCCESS;

			auto fnParams = nos::NodeExecuteParams(functionExecParams);

			auto transitionType = (EZDTransitionType)*fnParams.GetPinData<u32>(NSN_Type);
			auto transitionTarget = (EZDTransitionTarget)*fnParams.GetPinData<u32>(NSN_Target);

			if (transitionType == EZDTransitionType::Cut) {
				if (transitionTarget == EZDTransitionTarget::Program) {
					nosEngine.SetPinValueByName(context->NodeId, NSN_ProgramChannel, { &previewChannelIndex, sizeof(u32) });
					nosEngine.SetPinValueByName(context->NodeId, NSN_PreviewChannel, { &programChannelIndex, sizeof(u32) });
				}
				return NOS_RESULT_SUCCESS;
			}

			context->TransitionDuration = *fnParams.GetPinData<float>(NSN_Duration);
			context->WipeWidth = *fnParams.GetPinData<float>(NSN_WipeWidth);
			context->TransitionInterp = (EZDTransitionInterpolation)*fnParams.GetPinData<u32>(NSN_Interpolation);
			context->TransitionEaseExponent = *fnParams.GetPinData<float>(NSN_EaseExponent);
			context->TransitionStepCount = *fnParams.GetPinData<u32>(NSN_StepCount);
			context->TransitionType = transitionType;
			context->TransitionTarget = transitionTarget;
			context->TransitionProgramChannel = programChannelIndex;
			context->TransitionPreviewChannel = previewChannelIndex;
			context->TransitionStart = std::chrono::high_resolution_clock::now();
			context->bIsInTransition = true;
			nosEngine.SetPinValueByName(context->NodeId, NSN_IsInTransition, { &context->bIsInTransition, sizeof(context->bIsInTransition) });
			return NOS_RESULT_SUCCESS;
		}
	};


	void RegisterMixerNode(nosNodeFunctions* nodeFunctions)
	{
		NOS_BIND_NODE_CLASS(NOS_NAME_STATIC("Mixer"), MixerContext, nodeFunctions);
	}

} // namespace nos
