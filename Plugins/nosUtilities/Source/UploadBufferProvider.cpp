// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>

// External
#include <nosVulkanSubsystem/Helpers.hpp>
#include <Nodos/Utils/Stopwatch.hpp>

NOS_REGISTER_NAME(Buffer);
NOS_REGISTER_NAME(GPUEventRef);
NOS_REGISTER_NAME(QueueSize);
NOS_REGISTER_NAME(BufferSize);
NOS_REGISTER_NAME(Alignment);
NOS_REGISTER_NAME(ForceHostMemory);
NOS_REGISTER_NAME(UseHostCachedMemory);

namespace nos::utilities
{
struct UploadBuffer
{
	TypedObjectRef<sys::vulkan::Buffer> Buffer{};
	TypedObjectRef<sys::vulkan::GPUEventHolder> DownloadCompleteEventHolder{};
	UploadBuffer(nosBufferInfo sampleBufferInfo) : Buffer(sys::vulkan::CreateBuffer(sampleBufferInfo, "UploadBuffer"))
	{
		nosVulkan->CreateGPUEventHolder(&DownloadCompleteEventHolder.GetStorage());
	}
	UploadBuffer(const UploadBuffer& other) = delete;
	UploadBuffer& operator=(const UploadBuffer& other) = delete;
	UploadBuffer(UploadBuffer&&) = default;
	UploadBuffer& operator=(UploadBuffer&&) = default;

	~UploadBuffer()
	{
		if (DownloadCompleteEventHolder)
		{
			nosGPUEvent* event = nullptr;
			auto res = nosVulkan->GetGPUEventFromHolder(DownloadCompleteEventHolder, &event);

			if (res == NOS_RESULT_SUCCESS && *event)
				nosVulkan->WaitGpuEvent(event, UINT64_MAX);
		}
	}
};

struct UploadBufferProviderNode : NodeContext
{
	nosBufferInfo SampleBufferInfo = {
		.Size = 0,
		.Usage = nosBufferUsage(NOS_BUFFER_USAGE_TRANSFER_SRC),
		.MemoryFlags = nosMemoryFlags(NOS_MEMORY_FLAGS_HOST_VISIBLE | NOS_MEMORY_FLAGS_FORCE_HOST_MEMORY)};
	std::vector<UploadBuffer> Buffers;
	uint64_t QueueSize = 2;

	size_t CurrentIndex = 0;

	nosResult OnCreate(nosFbNodePtr node) override
	{
		AddPinValueWatcher<uint32_t>(NSN_QueueSize, [this](const uint32_t* newVal, auto) {
			QueueSize = *newVal;
			if (QueueSize == 0)
				return;
			if (Buffers.size() == QueueSize)
				return;
			if (SampleBufferInfo.Size == 0)
				return;
			Buffers.clear();
			for (size_t i = 0; i < QueueSize; i++)
				Buffers.emplace_back(SampleBufferInfo);
			CurrentIndex = 0;
		});
		AddPinValueWatcher<uint64_t>(NSN_BufferSize, [this](const uint64_t* newSize, auto) {
			if (*newSize == 0)
				return;
			if (SampleBufferInfo.Size == *newSize)
				return;
			SampleBufferInfo.Size = *newSize;
			Buffers.clear();
			for (size_t i = 0; i < QueueSize; i++)
				Buffers.emplace_back(SampleBufferInfo);
		});
		AddPinValueWatcher<uint64_t>(NSN_Alignment, [this](const uint64_t* newAlignment, auto) {
			if (SampleBufferInfo.Alignment == *newAlignment)
				return;
			SampleBufferInfo.Alignment = *newAlignment;
			Buffers.clear();
			if (SampleBufferInfo.Size == 0)
				return;
			for (size_t i = 0; i < QueueSize; i++)
				Buffers.emplace_back(SampleBufferInfo);
		});
		AddPinValueWatcher<bool>(NSN_ForceHostMemory, [this](const bool* newForceHostMemory, auto) {
			auto& memFlags = SampleBufferInfo.MemoryFlags;
			if (!!(memFlags & NOS_MEMORY_FLAGS_FORCE_HOST_MEMORY) == *newForceHostMemory)
				return;
			if (*newForceHostMemory)
				memFlags = nosMemoryFlags(memFlags | NOS_MEMORY_FLAGS_FORCE_HOST_MEMORY);
			else
				memFlags = nosMemoryFlags(memFlags & ~NOS_MEMORY_FLAGS_FORCE_HOST_MEMORY);
			Buffers.clear();
			if (SampleBufferInfo.Size == 0)
				return;
			for (size_t i = 0; i < QueueSize; i++)
				Buffers.emplace_back(SampleBufferInfo);
		});
		AddPinValueWatcher<bool>(NSN_UseHostCachedMemory, [this](const bool* newHostCached, auto) {
			auto& memFlags = SampleBufferInfo.MemoryFlags;
			if (!!(memFlags & NOS_MEMORY_FLAGS_DOWNLOAD) == *newHostCached)
				return;
			if (*newHostCached)
				memFlags = nosMemoryFlags(memFlags | NOS_MEMORY_FLAGS_DOWNLOAD);
			else
				memFlags = nosMemoryFlags(memFlags & ~NOS_MEMORY_FLAGS_DOWNLOAD);
			Buffers.clear();
			if (SampleBufferInfo.Size == 0)
				return;
			for (size_t i = 0; i < QueueSize; i++)
				Buffers.emplace_back(SampleBufferInfo);
		});
		return NOS_RESULT_SUCCESS;
	}

	nosResult ExecuteNode(NodeExecuteParams const& params) override
	{
		if (Buffers.size() == 0)
			return NOS_RESULT_FAILED;
		auto& nextBuf = Buffers[CurrentIndex];
		CurrentIndex = (CurrentIndex + 1) % QueueSize;
		if (nextBuf.DownloadCompleteEventHolder.IsValid())
		{
			util::Stopwatch sw;
			nosGPUEvent* event = nullptr;
			auto res = nosVulkan->GetGPUEventFromHolder(nextBuf.DownloadCompleteEventHolder, &event);
			if (*event)
				nosVulkan->WaitGpuEvent(event, UINT64_MAX);
			nosEngine.WatchLog("AsyncDowloadBuffer Wait", sw.ElapsedString().c_str());
		}

		SetPinObject(NSN_Buffer, nextBuf.Buffer);
		SetPinObject(NSN_GPUEventRef, nextBuf.DownloadCompleteEventHolder);

		return NOS_RESULT_SUCCESS;
	}

	void OnPathStop() override
	{
		for (auto& buf : Buffers)
		{
			if (buf.DownloadCompleteEventHolder.IsValid())
			{
				nosGPUEvent* event = nullptr;
				auto res = nosVulkan->GetGPUEventFromHolder(buf.DownloadCompleteEventHolder, &event);
				if (*event)
					nosVulkan->WaitGpuEvent(event, UINT64_MAX);
			}
		}
		CurrentIndex = 0;
	}

	void OnPathCommand(const nosPathCommand* command) override
	{
		switch (command->Event)
		{
		case NOS_RING_SIZE_CHANGE: {
			if (command->RingSize == 0)
			{
				nosEngine.LogW("Buffer provider size cannot be 0");
				return;
			}
			nosEngine.SetPinValue(PinName2Id[NSN_QueueSize], Buffer::From(uint32_t(command->RingSize)));
			break;
		}
		default: return;
		}
	}
};

nosResult RegisterUploadBufferProvider(nosNodeFunctions* functions)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("UploadBufferProvider"), UploadBufferProviderNode, functions)
	return NOS_RESULT_SUCCESS;
}
} // namespace nos::utilities