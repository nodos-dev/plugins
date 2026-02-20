// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include "Names.h"
#include <nosSysVulkan/Helpers.hpp>
#include <glm/vec2.hpp>

namespace nos::filters
{

NOS_REGISTER_NAME(FramesCount);
NOS_REGISTER_NAME(TEMPORAL_BLUR_FSHADER);
NOS_REGISTER_NAME(TEMPORAL_BLUR_PASS);
NOS_REGISTER_NAME(History);

// update texture size and format if needed
bool UpdateTextureFormat(nos::ObjectRef& texture, glm::uvec2 res, const char* tag, nosFormat format)
{
	auto textureInfo = nos::sys::vulkan::GetResourceInfo(texture).value_or({});
	if (format == NOS_FORMAT_NONE)
		format = textureInfo.Texture.Format;

	if (textureInfo.Texture.Width == res.x &&
		textureInfo.Texture.Height == res.y &&
		textureInfo.Texture.Format == format)
		return false;

	nosResourceInfo newTexture = {};
	newTexture.Type = NOS_RESOURCE_TYPE_TEXTURE;
	newTexture.Texture.Usage = NOS_IMAGE_USAGE_TRANSFER_DST;
	newTexture.Texture.Width = res.x;
	newTexture.Texture.Height = res.y;
	newTexture.Texture.Format = format;

	texture = {};
	return nosVulkan->CreateResource(&newTexture, 0, tag, &texture.GetStorage()) == NOS_RESULT_SUCCESS;
}

struct TemporalBlurContext : public NodeContext
{
	static const int MAX_FRAMES = 8;
	std::vector<nos::ObjectRef> History{MAX_FRAMES};

	int WriteId = 0;
public:

	virtual nosResult ExecuteNode(nos::NodeExecuteParams const& params) override
	{

		auto inputTexture  = params.GetPinObject(NSN_In);
		auto outputTexture = params.GetPinObject(NSN_Out);

		auto inputTextureInfo  = *nos::sys::vulkan::GetResourceInfo(inputTexture);
		auto outputTextureInfo = *nos::sys::vulkan::GetResourceInfo(outputTexture);
		for (int i = 0; i < MAX_FRAMES; ++i)
		{
			UpdateTextureFormat(History[i], { inputTextureInfo.Texture.Width, inputTextureInfo.Texture.Height }, "History texture", inputTextureInfo.Texture.Format);
		}

		int framesCount = *params.GetPinData<int>(NSN_FramesCount);

		nosCmd cmd;
		nosCmdBeginParams bp = {.Name = nos::Name("Copy History"), .AssociatedNodeId = NodeId, .OutCmdHandle = &cmd};

		nosVulkan->Begin(&bp);
		nosVulkan->Copy(cmd, inputTexture, History[WriteId], nullptr);

		nosRunPassParams pass = {};
		pass.Key = NSN_TEMPORAL_BLUR_PASS;

		std::vector<nosTextureObject> textures{MAX_FRAMES};
		std::vector<nosTextureFilter> filters{MAX_FRAMES};
		
		for (int i = 0; i < MAX_FRAMES; ++i)
		{
			textures[i] = History[(MAX_FRAMES + WriteId - i) % MAX_FRAMES];
			filters[i] = NOS_TEXTURE_FILTER_LINEAR;
		}

		std::vector<nosShaderBinding> bindings{
			nos::sys::vulkan::ShaderTextureArrayBinding(NSN_History, textures.data(), filters.data(), (u32)textures.size()),
			nos::sys::vulkan::ShaderDataBinding(NSN_FramesCount, framesCount)
		};

		pass.Bindings = bindings.data();
		pass.BindingCount = (u32)bindings.size();
		pass.Output = outputTexture;
		nosVulkan->RunPass(cmd, &pass);

		nosVulkan->End(cmd, 0);

		WriteId = (WriteId + 1) % MAX_FRAMES;
		return NOS_RESULT_SUCCESS;
	}

};

void RegisterTemporalBlurNode(nosNodeFunctions* nodeFunctions)
{
	NOS_BIND_NODE_CLASS(NOS_NAME_STATIC("TemporalBlur"), TemporalBlurContext, nodeFunctions);
}

}
