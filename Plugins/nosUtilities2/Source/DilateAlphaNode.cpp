// Copyright Zero Density AS. All Rights Reserved.
#include "Common.h"

namespace nos
{
    NOS_REGISTER_NAME(TexelSize); // vec2 shader parameter
    NOS_REGISTER_NAME(Direction); // vec2 shader parameter
    NOS_REGISTER_NAME(Radius); // vec2 shader parameter
    NOS_REGISTER_NAME(DILATE_ALPHA_PASS);

    struct DilateAlphaContext : public NodeContext
    {

        nos::ObjectRef SetupIntermediateTexture(nosResourceInfo* outputTexture)
        {
            nosResourceInfo info = {};
            info.Type = NOS_RESOURCE_TYPE_TEXTURE;
            info.Texture.Usage = nosImageUsage(NOS_IMAGE_USAGE_RENDER_TARGET | NOS_IMAGE_USAGE_SAMPLED);
            info.Texture.Width = outputTexture->Texture.Width;
            info.Texture.Height = outputTexture->Texture.Height;
            info.Texture.Format = outputTexture->Texture.Format;

            nos::ObjectRef object;
            nosVulkan->CreateResource(&info, 0, "DilateAlpha_IntermediateTexture", &object.GetStorage());
            return object;
        }


        nosResult ExecuteNode(nos::NodeExecuteParams const& params)
        {
			//if (!HasPinValues(params, NSN_Out, NSN_In, NSN_Radius))
			//	return NOS_RESULT_INVALID_ARGUMENT;

            // auto values = GetPinValues(params);

            auto outInfo = *nos::sys::vulkan::GetResourceInfo(params.GetPinObject(NSN_Out));
            auto intermediateTexture = SetupIntermediateTexture(&outInfo);

            nos::fb::vec2 texelSize(1.f / outInfo.Texture.Width, 1.f / outInfo.Texture.Height);
            nos::fb::vec2 horizontal(1.0, 0.0);
            nos::fb::vec2 vertical(0.0, 1.0);
            fb::vec2 radius = *params.GetPinData<fb::vec2>(NSN_Radius);


            nosCmd cmd;
			nosCmdBeginParams bp = {.Name = nos::Name("DilateAlpha"), .AssociatedNodeId = NodeId, .OutCmdHandle = &cmd};
			nosVulkan->Begin(&bp);

            // Pass horizontal begin
            nosRunPassParams dilateAlphaHorizontal = {};
            dilateAlphaHorizontal.Key = NSN_DILATE_ALPHA_PASS;
            std::vector horizontalBindings = {
              nos::sys::vulkan::ShaderTextureBinding(NSN_In, params.GetPinObject(NSN_In), NOS_TEXTURE_FILTER_LINEAR),
              nos::sys::vulkan::ShaderDataBinding(NSN_TexelSize, texelSize),
              nos::sys::vulkan::ShaderDataBinding(NSN_Direction, horizontal),
              nos::sys::vulkan::ShaderDataBinding(NSN_Radius, radius)
            };

            dilateAlphaHorizontal.Bindings = horizontalBindings.data();
            dilateAlphaHorizontal.BindingCount = (u32)horizontalBindings.size();
            dilateAlphaHorizontal.Output = intermediateTexture;
            nosVulkan->RunPass(cmd, &dilateAlphaHorizontal);

            // Pass vertical begin
            nosRunPassParams dilateAlphaVertical = {};
            dilateAlphaVertical.Key = NSN_DILATE_ALPHA_PASS;
            std::vector verticalBindings = {
              nos::sys::vulkan::ShaderTextureBinding(NSN_In, intermediateTexture, NOS_TEXTURE_FILTER_LINEAR),
              nos::sys::vulkan::ShaderDataBinding(NSN_TexelSize, texelSize),
              nos::sys::vulkan::ShaderDataBinding(NSN_Direction, vertical),
              nos::sys::vulkan::ShaderDataBinding(NSN_Radius, radius)
            };

            dilateAlphaVertical.Bindings = verticalBindings.data();
            dilateAlphaVertical.BindingCount = (u32)verticalBindings.size();
            dilateAlphaVertical.Output = params.GetPinObject(NSN_Out);
            nosVulkan->RunPass(cmd, &dilateAlphaVertical);

            nosVulkan->End(cmd, 0);
			return NOS_RESULT_SUCCESS;
        }
    };

    void RegisterDilateAlphaNode(nosNodeFunctions* nodeFunctions)
    {
		NOS_BIND_NODE_CLASS(NOS_NAME_STATIC("DilateAlpha"), DilateAlphaContext, nodeFunctions);
    }

} // namespace nos
