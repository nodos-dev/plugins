// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>
#include <nosSysVulkan/Helpers.hpp>

namespace nos::resource
{

struct RGBA2BGR24BufferNodeContext : NodeContext
{
	nosResult ExecuteNode(NodeExecuteParams const& params) override
	{
		auto inputTex = params.GetPinObject<sys::vulkan::Texture>(NOS_NAME_STATIC("Source"));
		auto outputBuf = params.GetPinObject<sys::vulkan::Buffer>(NOS_NAME_STATIC("Output"));
		auto inputTexInfo = *sys::vulkan::GetResourceInfo(inputTex);
		auto outputBufInfo = sys::vulkan::GetResourceInfo(outputBuf);

		nosVec2u ext = {inputTexInfo.Width, inputTexInfo.Height};

		uint32_t bufSize = ext.x * ext.y * 3;
		constexpr auto outMemoryFlags = NOS_MEMORY_FLAGS_DEVICE_MEMORY;
		if (!outputBufInfo || outputBufInfo->Size != bufSize || outputBufInfo->MemoryFlags != outMemoryFlags)
		{
			auto bufObj = sys::vulkan::CreateBuffer(
				{
					.Size = static_cast<uint32_t>(bufSize),
					.Usage = nosBufferUsage(NOS_BUFFER_USAGE_TRANSFER_SRC | NOS_BUFFER_USAGE_STORAGE_BUFFER),
					.MemoryFlags = outMemoryFlags,
				},
				"RGBA2BGR24Result");
			SetPinObject(NOS_NAME_STATIC("Output"), bufObj);
			nosVulkan->SetResourceFieldType(bufObj, nosTextureFieldType::NOS_TEXTURE_FIELD_TYPE_PROGRESSIVE);
		}
		else
		{
			nosVulkan->SetResourceFieldType(outputBuf, nosTextureFieldType::NOS_TEXTURE_FIELD_TYPE_PROGRESSIVE);
		}

		SetPinValue(NOS_NAME("DispatchSize"), nosVec2u(ext.x / 4, ext.y));
		return nosVulkan->ExecuteGPUNode(this, params.RawParams);
	}
};

nosResult RegisterRGBAToBGR24Buffer(nosNodeFunctions* funcs)
{
	NOS_BIND_NODE_CLASS(NOS_NAME_STATIC("nos.resource.RGBAToBGR24Buffer"), RGBA2BGR24BufferNodeContext, funcs);
	return NOS_RESULT_SUCCESS;
}

} // namespace nos::resource
