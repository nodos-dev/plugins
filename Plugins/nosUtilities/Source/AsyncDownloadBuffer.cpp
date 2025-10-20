// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>

// External
#include <nosVulkanSubsystem/Helpers.hpp>
#include <Nodos/Utils/Stopwatch.hpp>

#include <nosSync/nosSync.h>
#include <nosSync/Sync_generated.h>

NOS_REGISTER_NAME(Buffer);
NOS_REGISTER_NAME(GPUEventRef);
NOS_REGISTER_NAME(QueueSize);
NOS_REGISTER_NAME(BufferSize);
NOS_REGISTER_NAME(Alignment);
NOS_REGISTER_NAME(ForceHostMemory);
NOS_REGISTER_NAME(UseHostCachedMemory);

namespace nos::utilities
{
struct AsyncDownloadBuffer
{
	TypedObjectRef<sys::vulkan::Buffer> Buffer{};
	TypedObjectRef<sys::vulkan::GPUEventHolder> DownloadCompleteEventHolder{};
	TypedObjectRef<sync::Promise> TransferCompletePromise{};

	AsyncDownloadBuffer(nosBufferInfo sampleBufferInfo) : Buffer(sys::vulkan::CreateBuffer(sampleBufferInfo, "AsyncDownloadBuffer"))
	{
		nosVulkan->CreateGPUEventHolder(&DownloadCompleteEventHolder.GetStorage());
		nosSync->CreatePromise("AsyncDownloadBuffer Transfer Complete", &TransferCompletePromise.GetStorage());
	}
	AsyncDownloadBuffer(const AsyncDownloadBuffer& other) = delete;
	AsyncDownloadBuffer& operator=(const AsyncDownloadBuffer& other) = delete;
	AsyncDownloadBuffer(AsyncDownloadBuffer&&) = default;
	AsyncDownloadBuffer& operator=(AsyncDownloadBuffer&&) = default;


	~AsyncDownloadBuffer()
	{
	}
};

struct AsyncDownloadBufferNode : NodeContext
{
	nosBufferInfo SampleBufferInfo = {
		.Size = 0,
		.Usage = nosBufferUsage(NOS_BUFFER_USAGE_TRANSFER_DST),
		.MemoryFlags = nosMemoryFlags(NOS_MEMORY_FLAGS_HOST_VISIBLE | NOS_MEMORY_FLAGS_FORCE_HOST_MEMORY)};
	std::vector<AsyncDownloadBuffer> Buffers;
	uint64_t QueueSize = 2;

	size_t CurrentIndex = 0;

	TypedObjectRef<sys::vulkan::Buffer> CurrentInput;

    void RecreateBuffers()
    {
		Buffers.clear();
		for (size_t i = 0; i < QueueSize; i++)
			Buffers.emplace_back(SampleBufferInfo);
		CurrentIndex = 0;
    }

	nosResult OnCreate(nosFbNodePtr node) override
	{
		AddPinValueWatcher<uint32_t>(NSN_QueueSize, [this](uint32_t* newVal, auto) {
			QueueSize = *newVal;
			if (QueueSize == 0)
				return;
			if (Buffers.size() == QueueSize)
				return;
			if (SampleBufferInfo.Size == 0)
				return;
			RecreateBuffers();
		});
		AddPinObjectWatcher<sys::vulkan::Buffer>(NOS_NAME("Input"), [this](TypedObjectRef<sys::vulkan::Buffer> const& newInput, auto) {
			CurrentInput = newInput;
			if (!newInput)
				return;
			auto sampleInfo = sys::vulkan::GetResourceInfo(CurrentInput);
			if (!sampleInfo)
				return;
			auto alignment = SampleBufferInfo.Alignment;
			auto memflags = SampleBufferInfo.MemoryFlags;
			SampleBufferInfo = *sampleInfo;
			SampleBufferInfo.Alignment = alignment;
			SampleBufferInfo.MemoryFlags = memflags;
			RecreateBuffers();
		});
		AddPinValueWatcher<uint64_t>(NSN_Alignment, [this](uint64_t* newAlignment, auto) {
			if (SampleBufferInfo.Alignment == *newAlignment)
				return;
			SampleBufferInfo.Alignment = *newAlignment;
			if (SampleBufferInfo.Size == 0)
				return;
			RecreateBuffers();
		});
		AddPinValueWatcher<bool>(NSN_ForceHostMemory, [this](bool* newForceHostMemory, auto) {
			auto& memFlags = SampleBufferInfo.MemoryFlags;
			if (!!(memFlags & NOS_MEMORY_FLAGS_FORCE_HOST_MEMORY) == *newForceHostMemory)
				return;
			if (*newForceHostMemory)
				memFlags = nosMemoryFlags(memFlags | NOS_MEMORY_FLAGS_FORCE_HOST_MEMORY);
			else
				memFlags = nosMemoryFlags(memFlags & ~NOS_MEMORY_FLAGS_FORCE_HOST_MEMORY);
			if (SampleBufferInfo.Size == 0)
				return;
			RecreateBuffers();
		});
		AddPinValueWatcher<bool>(NSN_UseHostCachedMemory, [this](bool* newHostCached, auto) {
			auto& memFlags = SampleBufferInfo.MemoryFlags;
			if (!!(memFlags & NOS_MEMORY_FLAGS_DOWNLOAD) == *newHostCached)
				return;
			if (*newHostCached)
				memFlags = nosMemoryFlags(memFlags | NOS_MEMORY_FLAGS_DOWNLOAD);
			else
				memFlags = nosMemoryFlags(memFlags & ~NOS_MEMORY_FLAGS_DOWNLOAD);
			if (SampleBufferInfo.Size == 0)
				return;
			RecreateBuffers();
		});
		return NOS_RESULT_SUCCESS;
	}

	nosResult ExecuteNode(NodeExecuteParams const& params) override
	{
		if (Buffers.size() == 0)
			return NOS_RESULT_FAILED;
		auto& nextBuf = Buffers[CurrentIndex];
		CurrentIndex = (CurrentIndex + 1) % QueueSize;
		nosCmd cmd{};
		nosGPUEvent* downloadCompleteEvent{};
		nosCmdBeginParams beginParams{
			.Name = NOS_NAME("AsyncDownloadBuffer Copy"),
			.AssociatedNodeId = NodeId,
			.OutCmdHandle = &cmd,
		};
		nosVulkan->Begin(&beginParams);
		if (nextBuf.DownloadCompleteEventHolder.IsValid())
		{
			nosVulkan->GetGPUEventFromHolder(nextBuf.DownloadCompleteEventHolder, &downloadCompleteEvent);
		}
		nosVulkan->Copy(cmd, CurrentInput, nextBuf.Buffer, nullptr);
		nosCmdEndParams endParams{
			.OutGPUEventHandle = downloadCompleteEvent,
		};
		nosVulkan->End(cmd, &endParams);

		SetPinObject(NSN_Buffer, nextBuf.Buffer);
		SetPinObject(NOS_NAME("DownloadCompleteGPUEvent"), nextBuf.DownloadCompleteEventHolder);
    	SetPinObject(NOS_NAME("TransferCompletePromise"), nextBuf.TransferCompletePromise);

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

nosResult RegisterAsyncDownloadBuffer(nosNodeFunctions* functions)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("AsyncDownloadBuffer"), AsyncDownloadBufferNode, functions)
	return NOS_RESULT_SUCCESS;
}
} // namespace nos::utilities