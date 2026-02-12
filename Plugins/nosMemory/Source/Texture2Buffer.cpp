// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>

// External
#include <nosSysVulkan/Helpers.hpp>

#include "Names.h"

namespace nos::memory
{
NOS_REGISTER_NAME(Texture2Buffer)
NOS_REGISTER_NAME(OutputBuffer)

nosBufferElementType GetBufferElementTypeFromVulkanFormat(nosFormat format);

struct Texture2BufferNode : nos::NodeContext
{
	nosResult ExecuteNode(NodeExecuteParams const& params) override
	{
		auto inTex = params.GetPinObject<sys::vulkan::Texture>(NSN_Input);
		if (!inTex.IsValid())
			return NOS_RESULT_FAILED;
		auto inTexInfo = *sys::vulkan::GetResourceInfo(inTex);
		auto inputFieldType = sys::vulkan::GetResourceFieldType(inTex);
		auto outBuf = params.GetPinObject<sys::vulkan::Buffer>(NSN_OutputBuffer);
		auto outBufInfo = sys::vulkan::GetResourceInfo(outBuf);
		uint32_t currentSize = static_cast<uint32_t>(inTexInfo.SizeInBytes);
		
		if (!outBufInfo || currentSize != outBufInfo->Size) {
			// Need to create a new buffer
			nosBufferInfo bufCreateInfo{
				.Size = currentSize,
				.Usage = nosBufferUsage(NOS_BUFFER_USAGE_TRANSFER_SRC | NOS_BUFFER_USAGE_TRANSFER_DST |
										NOS_BUFFER_USAGE_STORAGE_BUFFER),
				.ElementType = GetBufferElementTypeFromVulkanFormat(inTexInfo.Format),
			};
			outBuf = sys::vulkan::CreateBuffer(bufCreateInfo, "Texture2Buffer Output");
			SetPinObject(NSN_OutputBuffer, outBuf);
		}

		nosVulkan->SetResourceFieldType(outBuf, inputFieldType);

		nosCmd cmd = {};
		nosCmdBeginParams beginParams = {.Name = NOS_NAME("Texture2Buffer Copy"), .AssociatedNodeId = NodeId, .OutCmdHandle = &cmd};
		nosVulkan->Begin(&beginParams);
		nosVulkan->Copy(cmd, inTex, outBuf, 0);
		nosVulkan->End(cmd, nullptr);
		return NOS_RESULT_SUCCESS;
	}
};

nosResult RegisterTexture2Buffer(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NSN_Texture2Buffer, Texture2BufferNode, fn);
	return NOS_RESULT_SUCCESS;
}

// TODO: Maybe move it to nos.sys.vulkan subsystem
nosBufferElementType GetBufferElementTypeFromVulkanFormat(nosFormat format) {
	switch (format) {
		/*
		* VK_FORMAT_R8G8B8A8_SRGB specifies a four-component, 32-bit unsigned normalized format that
		has an 8-bit R component stored with sRGB nonlinear encoding in byte 0, an 8-bit G component stored with sRGB nonlinear
		encoding in byte 1, an 8-bit B component stored with sRGB nonlinear encoding in byte 2, and an 8-bit A component in byte 3.

		VK_FORMAT_B8G8R8A8_UNORM specifies a four-component, 32-bit unsigned normalized format that has an 8-bit B component in byte 0,
		an 8-bit G component in byte 1, an 8-bit R component in byte 2, and an 8-bit A component in byte 3.
		*/
	case NOS_FORMAT_R8_UNORM:
	case NOS_FORMAT_R8G8_UNORM:
	case NOS_FORMAT_R8G8B8_UNORM:
	case NOS_FORMAT_B8G8R8_UNORM:
	case NOS_FORMAT_R8G8B8A8_UNORM:
	case NOS_FORMAT_B8G8R8A8_UNORM:
	case NOS_FORMAT_G8B8G8R8_422_UNORM:
	case NOS_FORMAT_B8G8R8G8_422_UNORM:
	case NOS_FORMAT_R8_UINT:
	case NOS_FORMAT_R8G8_UINT:
	case NOS_FORMAT_B8G8R8_UINT:
	case NOS_FORMAT_R8G8B8A8_UINT:
	case NOS_FORMAT_R8_SRGB:
	case NOS_FORMAT_R8G8_SRGB:
	case NOS_FORMAT_R8G8B8_SRGB:
	case NOS_FORMAT_B8G8R8_SRGB:
	case NOS_FORMAT_R8G8B8A8_SRGB:
	case NOS_FORMAT_B8G8R8A8_SRGB:
		return NOS_BUFFER_ELEMENT_TYPE_UINT8;

	case NOS_FORMAT_R16_UNORM:
	case NOS_FORMAT_R16G16_UNORM:
	case NOS_FORMAT_R16G16B16_UNORM:
	case NOS_FORMAT_R16G16B16A16_UNORM:
	case NOS_FORMAT_D16_UNORM:
	case NOS_FORMAT_R16_UINT:
	case NOS_FORMAT_R16G16B16_UINT:
	case NOS_FORMAT_R16G16_UINT:
	case NOS_FORMAT_R16G16B16A16_UINT:
	case NOS_FORMAT_R16_USCALED:
	case NOS_FORMAT_R16G16_USCALED:
	case NOS_FORMAT_R16G16B16_USCALED:
	case NOS_FORMAT_R16G16B16A16_USCALED:
		return NOS_BUFFER_ELEMENT_TYPE_UINT16;

	case NOS_FORMAT_R16_SINT:
	case NOS_FORMAT_R16G16_SINT:
	case NOS_FORMAT_R16G16B16_SINT:
	case NOS_FORMAT_R16G16B16A16_SINT:
	case NOS_FORMAT_R16_SNORM:
	case NOS_FORMAT_R16G16_SNORM:
	case NOS_FORMAT_R16G16B16_SNORM:
	case NOS_FORMAT_R16G16B16A16_SNORM:
	case NOS_FORMAT_R16_SSCALED:
	case NOS_FORMAT_R16G16_SSCALED:
	case NOS_FORMAT_R16G16B16_SSCALED:
	case NOS_FORMAT_R16G16B16A16_SSCALED:
		return NOS_BUFFER_ELEMENT_TYPE_INT16;

	case NOS_FORMAT_R16_SFLOAT:
	case NOS_FORMAT_R16G16_SFLOAT:
	case NOS_FORMAT_R16G16B16_SFLOAT:
	case NOS_FORMAT_R16G16B16A16_SFLOAT:
		return NOS_BUFFER_ELEMENT_TYPE_FLOAT16;

	case NOS_FORMAT_R32_UINT:
	case NOS_FORMAT_R32G32_UINT:
	case NOS_FORMAT_R32G32B32_UINT:
	case NOS_FORMAT_R32G32B32A32_UINT:
		return NOS_BUFFER_ELEMENT_TYPE_UINT32;

	case NOS_FORMAT_R32_SINT:
	case NOS_FORMAT_R32G32_SINT:
	case NOS_FORMAT_R32G32B32_SINT:
	case NOS_FORMAT_R32G32B32A32_SINT:
		return NOS_BUFFER_ELEMENT_TYPE_INT32;

	case NOS_FORMAT_R32_SFLOAT:
	case NOS_FORMAT_R32G32_SFLOAT:
	case NOS_FORMAT_R32G32B32_SFLOAT:
	case NOS_FORMAT_R32G32B32A32_SFLOAT:
	case NOS_FORMAT_D32_SFLOAT:
		return NOS_BUFFER_ELEMENT_TYPE_FLOAT;

	default:
		return NOS_BUFFER_ELEMENT_TYPE_UNDEFINED;
	}
}
}
