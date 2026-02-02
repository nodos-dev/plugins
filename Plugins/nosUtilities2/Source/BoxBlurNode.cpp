// Copyright Zero Density AS. All Rights Reserved.
#include "Common.h"

namespace nos
{

NOS_REGISTER_NAME(BlurSize);	  // vec2 shader parameter
NOS_REGISTER_NAME(RedBlend);	  // float shader parameter
NOS_REGISTER_NAME(GreenBlend); // float shader parameter
NOS_REGISTER_NAME(BlueBlend);  // float shader parameter
NOS_REGISTER_NAME(AlphaBlend); // float shader parameter
NOS_REGISTER_NAME(Direction);  // vec2 shader parameter
NOS_REGISTER_NAME(TexelSize);  // vec2 shader parameter
NOS_REGISTER_NAME(BOX_BLUR_PASS);

struct BoxBlurContext : public NodeContext
{
    nos::ObjectRef IntermediateTexture;

	void SetupIntermediateTexture(nosResourceInfo* outputTexture)
	{
        if(IntermediateTexture.IsValid())
        {
            nosResourceInfo existingInfo = {};
            nosVulkan->GetResourceInfo(IntermediateTexture, &existingInfo, nullptr);
            if (existingInfo.Texture.Width == outputTexture->Texture.Width &&
                existingInfo.Texture.Height == outputTexture->Texture.Height &&
                existingInfo.Texture.Format == outputTexture->Texture.Format)
            {
                // Existing intermediate texture is valid
                return;
            }
            else
            {
                // Release existing intermediate texture
                IntermediateTexture = nos::ObjectRef();
            }
        }

		nosResourceInfo info = {};
		info.Type = NOS_RESOURCE_TYPE_TEXTURE;
		info.Texture.Usage = nosImageUsage(NOS_IMAGE_USAGE_RENDER_TARGET | NOS_IMAGE_USAGE_SAMPLED);
		info.Texture.Width = outputTexture->Texture.Width;
		info.Texture.Height = outputTexture->Texture.Height;
		info.Texture.Format = outputTexture->Texture.Format;

		nosVulkan->CreateResource(&info, 0, "BoxBlur_IntermediateTexture", &IntermediateTexture.GetStorage());
	}

	nosResult ExecuteNode(nos::NodeExecuteParams const& params)
	{
		if (!HasPinValues(params, NSN_Out, NSN_In, NSN_RedBlend, NSN_GreenBlend, NSN_BlueBlend, NSN_AlphaBlend))
			return NOS_RESULT_INVALID_ARGUMENT;

		auto outInfo = *nos::sys::vulkan::GetResourceInfo(params.GetPinObject(NSN_Out));
		SetupIntermediateTexture(&outInfo);

		float RedBlend   = *params.GetPinData<float>(NSN_RedBlend);
		float GreenBlend = *params.GetPinData<float>(NSN_GreenBlend);
		float BlueBlend  = *params.GetPinData<float>(NSN_BlueBlend);
		float AlphaBlend = *params.GetPinData<float>(NSN_AlphaBlend);

		nos::fb::vec2 texelSize(1.f / outInfo.Texture.Width,
								1.f / outInfo.Texture.Height);
		nos::fb::vec2 horizontal(1.0, 0.0);
		nos::fb::vec2 vertical(0.0, 1.0);
		fb::vec2 blurSize = *params.GetPinData<fb::vec2>(NSN_BlurSize);

		nosCmd cmd;
		nosCmdBeginParams bp = {.Name = nos::Name("Blur"), .AssociatedNodeId = NodeId, .OutCmdHandle = &cmd};
		nosVulkan->Begin(&bp);

		// Pass horizontal begin
		nosRunPassParams boxBlurHorizontal = {};
		boxBlurHorizontal.Key = NSN_BOX_BLUR_PASS;

		std::vector horizontalBindings = {
			nos::sys::vulkan::ShaderTextureBinding(NSN_In, params.GetPinObject(NSN_In), NOS_TEXTURE_FILTER_LINEAR),
			nos::sys::vulkan::ShaderDataBinding(NSN_BlurSize, blurSize),
			nos::sys::vulkan::ShaderDataBinding(NSN_RedBlend, RedBlend),
			nos::sys::vulkan::ShaderDataBinding(NSN_GreenBlend, GreenBlend),
			nos::sys::vulkan::ShaderDataBinding(NSN_BlueBlend, BlueBlend),
			nos::sys::vulkan::ShaderDataBinding(NSN_AlphaBlend, AlphaBlend),
			nos::sys::vulkan::ShaderDataBinding(NSN_Direction, horizontal),
			nos::sys::vulkan::ShaderDataBinding(NSN_TexelSize, texelSize)
		};

		boxBlurHorizontal.Bindings = horizontalBindings.data();
		boxBlurHorizontal.BindingCount = (u32)horizontalBindings.size();
		boxBlurHorizontal.Output = IntermediateTexture;
		nosVulkan->RunPass(cmd, &boxBlurHorizontal);

		// Pass vertical begin
		nosRunPassParams boxBlurVertical = {};
		boxBlurVertical.Key = NSN_BOX_BLUR_PASS;
		std::vector verticalBindings = {
			nos::sys::vulkan::ShaderTextureBinding(NSN_In, IntermediateTexture, NOS_TEXTURE_FILTER_LINEAR),
			nos::sys::vulkan::ShaderDataBinding(NSN_BlurSize, blurSize),
			nos::sys::vulkan::ShaderDataBinding(NSN_RedBlend, RedBlend),
			nos::sys::vulkan::ShaderDataBinding(NSN_GreenBlend, GreenBlend),
			nos::sys::vulkan::ShaderDataBinding(NSN_BlueBlend, BlueBlend),
			nos::sys::vulkan::ShaderDataBinding(NSN_AlphaBlend, AlphaBlend),
			nos::sys::vulkan::ShaderDataBinding(NSN_Direction, vertical),
			nos::sys::vulkan::ShaderDataBinding(NSN_TexelSize, texelSize)
		};

		boxBlurVertical.Bindings = verticalBindings.data();
		boxBlurVertical.BindingCount = (u32)verticalBindings.size();
		boxBlurVertical.Output = params.GetPinObject(NSN_Out);
		nosVulkan->RunPass(cmd, &boxBlurVertical);
		nosVulkan->End(cmd, 0);

		return NOS_RESULT_SUCCESS;
	}
};

void RegisterBoxBlurNode(nosNodeFunctions* nodeFunctions)
{
	NOS_BIND_NODE_CLASS(NOS_NAME_STATIC("nos.Utilities2.BoxBlur"), BoxBlurContext, nodeFunctions);

}

} // namespace nos
