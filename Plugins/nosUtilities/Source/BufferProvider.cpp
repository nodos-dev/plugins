// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>

// External
#include <nosVulkanSubsystem/Helpers.hpp>
#include <Nodos/Utils/Stopwatch.hpp>

#include <nosSync/nosSync.h>
#include <nosSync/Sync_generated.h>

NOS_REGISTER_NAME(Buffer);
NOS_REGISTER_NAME(GPUEventRef);
NOS_REGISTER_NAME(BufferCount);
NOS_REGISTER_NAME(BufferSize);
NOS_REGISTER_NAME(Alignment);
NOS_REGISTER_NAME(MemoryFlags);
NOS_REGISTER_NAME(Usage);
NOS_REGISTER_NAME(Input);

namespace nos::utilities
{
struct BufferSlot
{
	TypedObjectRef<sys::vulkan::Buffer> Buffer{};
	TypedObjectRef<sys::vulkan::GPUEventHolder> GPUEventHolder{};
	TypedObjectRef<sync::Promise> HostUseCompletePromise{};
	bool Served = false;
	BufferSlot(nosBufferInfo sampleBufferInfo, bool requireHostUseCompletion) : Buffer(sys::vulkan::CreateBuffer(sampleBufferInfo, "Buffer"))
	{
		nosVulkan->CreateGPUEventHolder(&GPUEventHolder.GetStorage());
		if (requireHostUseCompletion)
			nosSync->CreatePromise("Host Use Complete", &HostUseCompletePromise.GetStorage());
	}
	BufferSlot(const BufferSlot& other) = delete;
	BufferSlot& operator=(const BufferSlot& other) = delete;
	BufferSlot(BufferSlot&&) = default;
	BufferSlot& operator=(BufferSlot&&) = default;
	~BufferSlot()
	{
	}
};

struct BufferProviderNode : NodeContext
{
	nosBufferInfo SampleBufferInfo = {
		.Size = 0,
		.Usage = nosBufferUsage(NOS_BUFFER_USAGE_TRANSFER_DST),
		.MemoryFlags = nosMemoryFlags(NOS_MEMORY_FLAGS_HOST_VISIBLE | NOS_MEMORY_FLAGS_FORCE_HOST_MEMORY) };
	std::vector<BufferSlot> Buffers;
	uint64_t BufferCount = 2;
	bool ResizeOnPathRingSizeChanged = true;
	bool RequireHostUseCompletion = true;

	size_t CurrentIndex = 0;

	TypedObjectRef<sys::vulkan::Buffer> CurrentInput;

	void RecreateBuffers()
	{
		if (SampleBufferInfo.Size == 0)
			return;
		Buffers.clear();
		for (size_t i = 0; i < BufferCount; i++)
			Buffers.emplace_back(SampleBufferInfo, RequireHostUseCompletion);
		CurrentIndex = 0;
	}

	nosResult OnCreate(nosFbNodePtr node) override
	{
		AddPinValueWatcher<uint32_t>(NSN_BufferCount, [this](const uint32_t* newVal, auto) {
			BufferCount = *newVal;
			if (BufferCount == 0)
				return;
			if (Buffers.size() == BufferCount)
				return;
			RecreateBuffers();
		});
		AddPinValueWatcher<uint32_t>(NSN_Alignment, [this](const uint32_t* newAlignment, auto) {
			if (SampleBufferInfo.Alignment == *newAlignment)
				return;
			SampleBufferInfo.Alignment = *newAlignment;
			RecreateBuffers();
		});
		AddPinValueWatcher<uint64_t>(NSN_BufferSize, [this](const uint64_t* newSize, auto) {
			if (SampleBufferInfo.Size == *newSize)
				return;
			SampleBufferInfo.Size = *newSize;
			RecreateBuffers();
		});
		AddPinValueWatcher<nos::sys::vulkan::MemoryFlags>(
			NSN_MemoryFlags, [this](const nos::sys::vulkan::MemoryFlags* newMemoryFlags, auto) {
			auto newVal = nosMemoryFlags(*newMemoryFlags);
			if (SampleBufferInfo.MemoryFlags == newVal)
				return;
			SampleBufferInfo.MemoryFlags = newVal;
			RecreateBuffers();
		});
		AddPinValueWatcher<nos::sys::vulkan::BufferUsage>(NSN_Usage,
														  [this](const nos::sys::vulkan::BufferUsage* newUsage, auto) {
			auto newVal = nosBufferUsage(*newUsage);
			if (SampleBufferInfo.Usage == newVal)
				return;
			SampleBufferInfo.Usage = newVal;
			RecreateBuffers();
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
				RecreateBuffers();
			}
		});
		return NOS_RESULT_SUCCESS;
	}

	nosResult ExecuteNode(NodeExecuteParams const& params) override
	{
		if (Buffers.size() == 0)
			return NOS_RESULT_FAILED;
		auto& bufToServe = Buffers[CurrentIndex];
		if (bufToServe.Served)
		{
			if (bufToServe.HostUseCompletePromise)
			{
				auto res = nosSync->WaitPromise(bufToServe.HostUseCompletePromise, 100'000'000);
				if (res != NOS_RESULT_SUCCESS)
				{
					if (res == NOS_RESULT_TIMEOUT)
						nosEngine.LogW("%s: Timeout waiting for previous transfer to complete.", GetDisplayName().c_str());
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
							nosEngine.LogW("%s: Timeout waiting for GPU to complete operations on the buffer.", GetDisplayName().c_str());
						else
							nosEngine.LogE("%s: Failed waiting for GPU to complete operations on the buffer.", GetDisplayName().c_str());
					}
				}
			}
		}

		CurrentIndex = (CurrentIndex + 1) % BufferCount;
		SetPinObject(NOS_NAME("OutputBuffer"), bufToServe.Buffer);
		SetPinObject(NOS_NAME("GPUEventHolder"), bufToServe.GPUEventHolder);
		SetPinObject(NOS_NAME("HostUseCompletePromise"), bufToServe.HostUseCompletePromise);
		bufToServe.Served = true;

		return NOS_RESULT_SUCCESS;
	}

	void OnPathStop() override
	{
		for (auto& buf : Buffers)
		{
			if (buf.GPUEventHolder.IsValid())
			{
				nosGPUEvent* event = nullptr;
				auto res = nosVulkan->GetGPUEventFromHolder(buf.GPUEventHolder, &event);
				if (*event)
					nosVulkan->WaitGpuEvent(event, 100'000'000);
			}
		}
		RecreateBuffers();
		CurrentIndex = 0;
	}

	void OnPathCommand(const nosPathCommand* command) override
	{
		switch (command->Event)
		{
		case NOS_RING_SIZE_CHANGE: {
			if (!ResizeOnPathRingSizeChanged)
				return;
			if (command->RingSize == 0)
			{
				nosEngine.LogW("Buffer provider size cannot be 0");
				return;
			}
			nosEngine.SetPinValue(PinName2Id[NSN_BufferCount], Buffer::From(uint32_t(command->RingSize)));
			break;
		}
		default: return;
		}
	}
};

nosResult RegisterBufferProvider(nosNodeFunctions* functions)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("BufferProvider"), BufferProviderNode, functions)
	return NOS_RESULT_SUCCESS;
}
} // namespace nos::utilities