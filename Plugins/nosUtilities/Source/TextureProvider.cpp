// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>

// External
#include <nosVulkanSubsystem/Helpers.hpp>
#include <Nodos/Utils/Stopwatch.hpp>

#include <nosSync/nosSync.h>
#include <nosSync/Sync_generated.h>

NOS_REGISTER_NAME(OutputTexture);
NOS_REGISTER_NAME(GPUEventRef);
NOS_REGISTER_NAME(Count);
NOS_REGISTER_NAME(Resolution);
NOS_REGISTER_NAME(Usage);
NOS_REGISTER_NAME(Input);
NOS_REGISTER_NAME(Format);

namespace nos::utilities
{
struct TextureSlot
{
	TypedObjectRef<sys::vulkan::Texture> Texture{};
	TypedObjectRef<sys::vulkan::GPUEventHolder> GPUEventHolder{};
	bool Served = false;
	TextureSlot(nosTextureInfo sampleTextureInfo) : Texture(sys::vulkan::CreateTexture(sampleTextureInfo, "Texture"))
	{
		nosVulkan->CreateGPUEventHolder(&GPUEventHolder.GetStorage());
	}
	TextureSlot(const TextureSlot& other) = delete;
	TextureSlot& operator=(const TextureSlot& other) = delete;
	TextureSlot(TextureSlot&&) = default;
	TextureSlot& operator=(TextureSlot&&) = default;
	~TextureSlot()
	{
	}
};

struct TextureProviderNode : NodeContext
{
	nosTextureInfo SampleTextureInfo = {
		.Width = 0,
		.Height = 0,
		.Usage = nosImageUsage(NOS_IMAGE_USAGE_TRANSFER_DST | NOS_IMAGE_USAGE_TRANSFER_SRC | NOS_IMAGE_USAGE_SAMPLED),
	};
	std::vector<TextureSlot> Textures;
	uint64_t TextureCount = 2;

	size_t CurrentIndex = 0;

	TypedObjectRef<sys::vulkan::Buffer> CurrentInput;

	void RecreateTextures()
	{
		if (SampleTextureInfo.Width == 0 || SampleTextureInfo.Height == 0)
			return;
		Textures.clear();
		for (size_t i = 0; i < TextureCount; i++)
			Textures.emplace_back(SampleTextureInfo);
		CurrentIndex = 0;
	}

	nosResult OnCreate(nosFbNodePtr node) override
	{
		AddPinValueWatcher<uint32_t>(NSN_Count, [this](const uint32_t* newVal, auto) {
			TextureCount = *newVal;
			if (TextureCount == 0)
				return;
			if (Textures.size() == TextureCount)
				return;
			RecreateTextures();
		});
		AddPinValueWatcher<nos::fb::vec2u>(NSN_Resolution, [this](const nos::fb::vec2u* newSize, auto) {
			if (SampleTextureInfo.Width == newSize->x() && SampleTextureInfo.Height == newSize->y())
				return;
			SampleTextureInfo.Width = newSize->x();
			SampleTextureInfo.Height = newSize->y();
			RecreateTextures();
		});
		AddPinValueWatcher<nos::sys::vulkan::Format>(NSN_Format,
													 [this](const nos::sys::vulkan::Format* newFormat, auto) {
			if (SampleTextureInfo.Format == nosFormat(*newFormat))
				return;
			SampleTextureInfo.Format = nosFormat(*newFormat);
			RecreateTextures();
		});
		AddPinValueWatcher<nos::sys::vulkan::ImageUsage>(NSN_Usage,
														 [this](const nos::sys::vulkan::ImageUsage* newUsage, auto) {
			SampleTextureInfo.Usage = nosImageUsage(*newUsage);
			RecreateTextures();
		});
		return NOS_RESULT_SUCCESS;
	}

	nosResult ExecuteNode(NodeExecuteParams const& params) override
	{
		if (Textures.size() == 0)
			return NOS_RESULT_FAILED;
		auto& bufToServe = Textures[CurrentIndex];
		CurrentIndex = (CurrentIndex + 1) % TextureCount;
		SetPinObject(NOS_NAME("OutputTexture"), bufToServe.Texture);
		SetPinObject(NOS_NAME("GPUEventHolder"), bufToServe.GPUEventHolder);
		bufToServe.Served = true;

		return NOS_RESULT_SUCCESS;
	}

	void OnPathStop() override
	{
		for (auto& buf : Textures)
		{
			if (buf.GPUEventHolder.IsValid())
			{
				nosGPUEvent* event = nullptr;
				auto res = nosVulkan->GetGPUEventFromHolder(buf.GPUEventHolder, &event);
				if (*event)
					nosVulkan->WaitGpuEvent(event, 100'000'000);
			}
		}
		RecreateTextures();
		CurrentIndex = 0;
	}

	void OnPathCommand(const nosPathCommand* command) override
	{
		switch (command->Event)
		{
		case NOS_RING_SIZE_CHANGE: {
			if (command->RingSize == 0)
			{
				nosEngine.LogW("Texture provider size cannot be 0");
				return;
			}
			nosEngine.SetPinValue(PinName2Id[NSN_Count], Buffer::From(uint32_t(command->RingSize)));
			break;
		}
		default: return;
		}
	}
};

nosResult RegisterTextureProvider(nosNodeFunctions* functions)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("TextureProvider"), TextureProviderNode, functions)
	return NOS_RESULT_SUCCESS;
}
} // namespace nos::utilities