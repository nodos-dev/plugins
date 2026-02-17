// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>

// External
#include <nosSysVulkan/Helpers.hpp>
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

namespace nos::resource
{
struct TextureSlot
{
	TypedObjectRef<sys::vulkan::Texture> Texture{};
	TypedObjectRef<sys::vulkan::GPUEventHolder> GPUEventHolder{};
	TypedObjectRef<sync::Promise> HostUseCompletePromise{};
	bool Served = false;
	TextureSlot(nosTextureInfo sampleTextureInfo, bool requireHostUseCompletion)
		: Texture(sys::vulkan::CreateTexture(sampleTextureInfo, "Texture"))
	{
		nosVulkan->CreateGPUEventHolder(&GPUEventHolder.GetStorage());
		if (requireHostUseCompletion)
			nosSync->CreatePromise("Host Use Complete", &HostUseCompletePromise.GetStorage());
	}
	TextureSlot(const TextureSlot& other) = delete;
	TextureSlot& operator=(const TextureSlot& other) = delete;
	TextureSlot(TextureSlot&&) = default;
	TextureSlot& operator=(TextureSlot&&) = default;
	~TextureSlot()
	{
	}
};

// TODO: ResourceProvider node instead of separate Buffer/Texture providers
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

	bool RequireHostUseCompletion = true;
	bool ResizeOnPathRingSizeChanged = true;

	void RecreateTextures()
	{
		if (SampleTextureInfo.Width == 0 || SampleTextureInfo.Height == 0)
			return;
		Textures.clear();
		for (size_t i = 0; i < TextureCount; i++)
			Textures.emplace_back(SampleTextureInfo, RequireHostUseCompletion);
		CurrentIndex = 0;
	}

	nosResult OnCreate(nosFbNodePtr node) override
	{
		AddPinValueWatcher<uint32_t>(NSN_Count, [this](const uint32_t* newVal, auto) {
			TextureCount = std::max(*newVal, 1u);
			if (*newVal != TextureCount)
				SetPinValue(NOS_NAME("Count"), Buffer::From(TextureCount));
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
		AddPinValueWatcher<bool>(NOS_NAME("ResizeOnPathRingSizeChanged"), [this](const bool* newVal, auto) {
			ResizeOnPathRingSizeChanged = *newVal;
		});
		AddPinValueWatcher<bool>(NOS_NAME("RequireHostUseCompletion"), [this](const bool* newVal, auto) {
			if (RequireHostUseCompletion != *newVal)
			{
				RequireHostUseCompletion = *newVal;
				if (RequireHostUseCompletion)
					SetPinOrphanState(NOS_NAME("HostUseCompletePromise"), fb::PinOrphanStateType::ACTIVE);
				else
					SetPinOrphanState(NOS_NAME("HostUseCompletePromise"), fb::PinOrphanStateType::ORPHAN, "Waiting on host use completion is disabled");
				RecreateTextures();
			}
		});
		return NOS_RESULT_SUCCESS;
	}

	nosResult ExecuteNode(NodeExecuteParams const& params) override
	{
		if (Textures.size() == 0)
			return NOS_RESULT_FAILED;
		auto& bufToServe = Textures[CurrentIndex];
		if (bufToServe.Served)
		{
			if (bufToServe.HostUseCompletePromise)
			{
				auto res = nosSync->WaitPromise(bufToServe.HostUseCompletePromise, 100'000'000);
				if (res != NOS_RESULT_SUCCESS)
				{
					if (res == NOS_RESULT_TIMEOUT)
					{
						nosEngine.LogW("%s: Timeout waiting for previous transfer to complete.",
									   GetDisplayName().c_str());
						return NOS_RESULT_PENDING;
					}
					else
						nosEngine.LogE("%s: Failed waiting for previous transfer to complete.", GetDisplayName().c_str());
				}
			}
			if (bufToServe.GPUEventHolder)
			{
				nosGPUEvent* event = nullptr;
				auto res = nosVulkan->GetGPUEventFromHolder(bufToServe.GPUEventHolder, &event);
				if (*event)
				{
					res = nosVulkan->WaitGpuEvent(event, 100'000'000);
					if (res != NOS_RESULT_SUCCESS)
					{
						if (res == NOS_RESULT_TIMEOUT)
						{
							nosEngine.LogW("%s: Timeout waiting for GPU to complete operations on the buffer.",
										   GetDisplayName().c_str());
							return NOS_RESULT_PENDING;
						}
						else
							nosEngine.LogE("%s: Failed waiting for GPU to complete operations on the buffer.",
										   GetDisplayName().c_str());
					}
				}
			}
		}
		CurrentIndex = (CurrentIndex + 1) % TextureCount;
		SetPinObject(NOS_NAME("OutputTexture"), bufToServe.Texture);
		SetPinObject(NOS_NAME("GPUEventHolder"), bufToServe.GPUEventHolder);
		SetPinObject(NOS_NAME("HostUseCompletePromise"), bufToServe.HostUseCompletePromise);
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
} // namespace nos::resource