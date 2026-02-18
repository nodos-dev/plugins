// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>

#include <nosSysVulkan/Helpers.hpp>

#include "Names.h"

namespace nos::imageprocessing
{

NOS_REGISTER_NAME(RESIZE_PASS);
NOS_REGISTER_NAME(Method);
NOS_REGISTER_NAME(Size);
NOS_REGISTER_NAME_SPACED(Nos_Utilities_Resize, "nos.imageprocessing.Resize")

struct ResizeNode : nos::NodeContext
{
	using nos::NodeContext::NodeContext;

	static nosResult MigrateNode(nosFbNodePtr node, nosBuffer* outBuffer)
	{
		fb::TNode fbNode;
		node->UnPackTo(&fbNode);
		for (auto& pin : fbNode.pins)
		{
			if (pin->name != "Output")
				continue;
			bool hasExtensionForUnscaledOutput = false;
			for (auto& ext : pin->extensions)
			{
				if (ext->type_name == "nos.sys.vulkan.TexturePinOptions")
				{
					hasExtensionForUnscaledOutput = true;
					break;
				}
			}
			if (!hasExtensionForUnscaledOutput)
			{
				nos::sys::vulkan::TTexturePinOptions texPinOptions;
				texPinOptions.unscaled = true;
				auto buffer = nos::Buffer::From(texPinOptions);
				auto ext = std::make_unique<fb::TPinExtension>();
				ext->type_name = "nos.sys.vulkan.TexturePinOptions";
				ext->data = buffer;
				ext->name = "texture_options";
				pin->extensions.emplace_back(std::move(ext));
			}
		}
		auto fbNodeBuffer = nos::EngineBuffer::CopyFrom(nos::Buffer::From(fbNode));
		*outBuffer = fbNodeBuffer.Release();
		return NOS_RESULT_SUCCESS;
	}

	nosResult ExecuteNode(nos::NodeExecuteParams const& nodeParams) override
	{
		auto inTex = nodeParams.GetPinObject<sys::vulkan::Texture>(NSN_Input);
		auto method = *nodeParams.GetPinData<uint32_t>(NSN_Method);

		auto outTex = nodeParams.GetPinObject<sys::vulkan::Texture>(NSN_Output);
		auto& size = *nodeParams.GetPinData<nosVec2u>(NSN_Size);

		auto outTexInfo = sys::vulkan::GetResourceInfo(outTex);

		if (!outTexInfo || size.x != outTexInfo->Width || size.y != outTexInfo->Height)
		{
			auto newTexInfo = outTexInfo.value_or(nosTextureInfo{});
			newTexInfo.Width = size.x;
			newTexInfo.Height = size.y;
			// TODO: Transfer output unscaled
			outTex = sys::vulkan::CreateTexture(newTexInfo, "Resize Output");
			SetPinObject(NOS_NAME("Output"), outTex);
		}

		// TODO: Transfer filter
		std::vector bindings = {sys::vulkan::ShaderTextureBindingFromPin(nodeParams[NSN_Input].Id, NSN_Input, inTex),
								sys::vulkan::ShaderDataBinding(NSN_Method, method)};

		nosRunPassParams resizeParam{
			.Key = NSN_RESIZE_PASS,
			.Bindings = bindings.data(),
			.BindingCount = 2,
			.Output = outTex,
			.Wireframe = 0,
			.Benchmark = 0,
		};

		nosCmd cmd;
		nosCmdBeginParams beginParams{
			.Name = NOS_NAME("Resize"), .AssociatedNodeId = NodeId, .OutCmdHandle = &cmd};
		nosVulkan->Begin(&beginParams);
		nosVulkan->RunPass(cmd, &resizeParam);
		nosVulkan->End(cmd, nullptr);

		return NOS_RESULT_SUCCESS;
	}
};

void RegisterResize(nosNodeFunctions* out)
{
	NOS_BIND_NODE_CLASS(NSN_Nos_Utilities_Resize, ResizeNode, out);
}

} // namespace nos::imageprocessing


