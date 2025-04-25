/*
 * Copyright MediaZ Teknoloji A.S. All Rights Reserved.
 */

#pragma once
#include <Nodos/PluginHelpers.hpp>

 // External
#include <nosVulkanSubsystem/Helpers.hpp>

#include "nosUtil/Stopwatch.hpp"

#define inputPinName NOS_NAME_STATIC("Input")
#define outputPinName NOS_NAME_STATIC("Output")
#define voidTypeName NOS_NAME_STATIC("nos.Generic")
#define vulkanBufferTypeName NOS_NAME_STATIC("nos.sys.vulkan.Buffer")
#define vulkanTextureTypeName NOS_NAME_STATIC("nos.sys.vulkan.Texture")

namespace nos
{
struct ResourceInterface {
	virtual ~ResourceInterface() = default;

	enum class ResourceType : uint32_t {
		GPUBuffer = 1,
		GPUTexture,
		CPUGeneric
	};
	nos::Buffer Sample;
	ResourceType Type;

	struct ResourceBase {
		virtual ~ResourceBase() = default;
		ResourceType ResourceType;
		std::atomic_uint64_t FrameNumber;
	};


	template<typename T>
	static T::Resource* GetResource(ResourceBase* res) {
		if (res->ResourceType == T::RESOURCE_TYPE) {
			return static_cast<T::Resource*>(res);
		}
		return nullptr;
	}

	template<typename T>
	static T::PinData* GetPinData(nosPinInfo& pin) {
		return static_cast<T::PinData*>(pin.Data);
	}

	ResourceInterface(ResourceType type) : Type(type) {}
	virtual rc<ResourceBase> CreateResource() = 0;
	virtual void Reset(ResourceBase* res) = 0;
	virtual void WaitForDownloadToEnd(ResourceBase* res, const std::string& nodeTypeName, const std::string& nodeDisplayName, nosCopyInfo* cpy) = 0;
	virtual void Copy(ResourceBase* res, nosCopyInfo* cpy, uuid NodeId) = 0;
	virtual nosResult Push(ResourceBase* r, void* pinInfo, nosNodeExecuteParams* params, nos::Name ringExecuteName, bool pushEventForCopyFrom) = 0;
	virtual void* GetPinInfo(nosPinInfo& pin, bool rejectFieldMismatch) = 0;
	// Returns false if resource is compatible with the current sample
	virtual bool CheckNewResource(nosName updateName, nosBuffer newVal, std::optional<nos::Buffer> oldVal) = 0;
	virtual bool BeginCopyFrom(ResourceBase* r, const nosBuffer& pinData, nos::Buffer& outPinVal) = 0;
	virtual void OnPathStart() {}
};

struct GPUTextureResource : ResourceInterface {
	static constexpr ResourceType RESOURCE_TYPE = ResourceInterface::ResourceType::GPUTexture;
	struct Resource : ResourceBase {
		vkss::Resource VkRes;
		struct {
			nosTextureFieldType FieldType = NOS_TEXTURE_FIELD_TYPE_UNKNOWN;
			nosGPUEvent WaitEvent = 0;
		} Params{};
		Resource(vkss::Resource res) : VkRes(std::move(res))
		{
			ResourceType = RESOURCE_TYPE;
		}
		~Resource()
		{
			if (Params.WaitEvent)
				nosVulkan->WaitGpuEvent(&Params.WaitEvent, UINT64_MAX);
		}
	};
	typedef nosResourceShareInfo PinData;
	nosTextureFieldType WantedField = NOS_TEXTURE_FIELD_TYPE_UNKNOWN;
	static constexpr nosTextureInfo SampleTexture = nosTextureInfo{
		.Width = 1920,
		.Height = 1080,
		.Format = NOS_FORMAT_R16G16B16A16_SFLOAT,
		.Usage = nosImageUsage(NOS_IMAGE_USAGE_TRANSFER_SRC | NOS_IMAGE_USAGE_TRANSFER_DST),
	};

	GPUTextureResource() : ResourceInterface(ResourceType::GPUTexture) {
		nosResourceShareInfo shareInfo;
		shareInfo.Info.Type = NOS_RESOURCE_TYPE_TEXTURE;
		shareInfo.Info.Texture = SampleTexture;
		Sample = nos::Buffer::From(vkss::ConvertTextureInfo(shareInfo));
	}
	rc<ResourceBase> CreateResource() override
	{
		if (auto texture = vkss::Resource::CreateWithSameInfo(vkss::DeserializeTextureInfo(Sample.Data()), "Texture Ring Resource"))
			return MakeShared<Resource>(std::move(*texture));
		return nullptr;
	}
	void Reset(ResourceBase* r) override
	{
		Resource* res = GetResource<GPUTextureResource>(r);
		if (res->Params.WaitEvent)
			nosVulkan->WaitGpuEvent(&res->Params.WaitEvent, UINT64_MAX);
		res->Params = {};
		res->FrameNumber = 0;
	}

	void OnPathStart() override
	{
		WantedField = NOS_TEXTURE_FIELD_TYPE_UNKNOWN;
	}

	void WaitForDownloadToEnd(ResourceBase* r, const std::string& nodeTypeName, const std::string& nodeDisplayName, nosCopyInfo* cpy) override {
		Resource* res = GetResource<GPUTextureResource>(r); 
		if (res->Params.WaitEvent) {
			nos::util::Stopwatch sw;
			nosVulkan->WaitGpuEvent(&res->Params.WaitEvent, UINT64_MAX);

			auto elapsed = sw.Elapsed();
			nosEngine.WatchLog((nodeTypeName + " Copy From GPU Wait: " + nodeDisplayName).c_str(),
				nos::util::Stopwatch::ElapsedString(elapsed).c_str());
		}

		nosEngine.SetPinValue(cpy->ID, nos::Buffer::From(vkss::ConvertTextureInfo(res->VkRes)));
	}

	void Copy(ResourceBase* r, nosCopyInfo* cpy, uuid NodeId) override {
		Resource* res = GetResource<GPUTextureResource>(r);
		nosResourceShareInfo outputResource = vkss::DeserializeTextureInfo(cpy->PinData->Data);
		nosCmd cmd;
		nosCmdBeginParams beginParams = { NOS_NAME("BoundedQueue"), NodeId, &cmd };
		nosVulkan->Begin(&beginParams);
		nosVulkan->Copy(cmd, &res->VkRes, &outputResource, 0);
		nosCmdEndParams end{ .ForceSubmit = NOS_TRUE, .OutGPUEventHandle = &res->Params.WaitEvent };
		nosVulkan->End(cmd, &end);

		nosTextureFieldType outFieldType = res->VkRes.Info.Texture.FieldType;
		auto output = vkss::DeserializeTextureInfo(cpy->PinData->Data);
		output.Info.Texture.FieldType = outFieldType;
		sys::vulkan::TTexture texDef = vkss::ConvertTextureInfo(output);
		texDef.unscaled = true;
		nosEngine.SetPinValue(cpy->ID, Buffer::From(texDef));
	}

	void* GetPinInfo(nosPinInfo& pin, bool rejectFieldMismatch) override {
		nosResourceShareInfo input = vkss::DeserializeTextureInfo(pin.Data->Data);
		nosTextureFieldType incomingField = input.Info.Texture.FieldType;

		if (!input.Memory.Handle)
			return nullptr;


		if (rejectFieldMismatch)
		{
			if (WantedField == NOS_TEXTURE_FIELD_TYPE_UNKNOWN)
				WantedField = incomingField;

			auto outInterlaced = vkss::IsTextureFieldTypeInterlaced(WantedField);
			auto inInterlaced = vkss::IsTextureFieldTypeInterlaced(incomingField);
			if ((inInterlaced && outInterlaced) && incomingField != WantedField)
			{
				nosEngine.LogW("Field mismatch. Waiting for a new frame.");
				return nullptr;
			}
			WantedField = vkss::FlippedField(WantedField);
		}

		return pin.Data->Data;
	}

	nosResult Push(ResourceBase* r, void* pinInfo, nosNodeExecuteParams* params, nos::Name ringExecuteName, bool pushEventForCopyFrom) override {
		Resource* res = GetResource<GPUTextureResource>(r);
		res->FrameNumber = params->FrameNumber;

		nosResourceShareInfo input = vkss::DeserializeTextureInfo(pinInfo);
		nosTextureFieldType incomingField = input.Info.Texture.FieldType;
		res->VkRes.Info.Texture.FieldType = incomingField;

		if (res->Params.WaitEvent)
		{
			nos::util::Stopwatch sw;
			nosVulkan->WaitGpuEvent(&res->Params.WaitEvent, UINT64_MAX);
			auto elapsed = sw.Elapsed();
			nosEngine.WatchLog((ringExecuteName.AsString() + " Execute GPU Wait: " + nos::Name(params->NodeName).AsString()).c_str(),
				nos::util::Stopwatch::ElapsedString(elapsed).c_str());
		}
		nosCmd cmd;
		nosCmdBeginParams beginParams;
		beginParams = { ringExecuteName, params->NodeId, &cmd };

		nosVulkan->Begin(&beginParams);
		nosVulkan->Copy(cmd, &input, &res->VkRes, 0);
		nosCmdEndParams end{ .ForceSubmit = NOS_TRUE, .OutGPUEventHandle = pushEventForCopyFrom ? &res->Params.WaitEvent : nullptr };
		nosVulkan->End(cmd, &end);
		return NOS_RESULT_SUCCESS;
	}

	bool CheckNewResource(nosName updateName, nosBuffer newVal, std::optional<nos::Buffer> oldVal) override {
		bool needsRecreation = false;
		auto textureInfo = vkss::ConvertTextureInfo(vkss::DeserializeTextureInfo(Sample.Data()));
		if (updateName == NOS_NAME_STATIC("Input"))
		{
			auto info = vkss::DeserializeTextureInfo(newVal.Data);
			if (textureInfo.format == (nos::sys::vulkan::Format)info.Info.Texture.Format &&
				textureInfo.height == info.Info.Texture.Height && textureInfo.width == info.Info.Texture.Width)
				return false;
			textureInfo.format = (nos::sys::vulkan::Format)info.Info.Texture.Format;
			textureInfo.width = info.Info.Texture.Width;
			textureInfo.height = info.Info.Texture.Height;
		}
		else
			return false;
		Sample = Buffer::From(textureInfo);
		return needsRecreation;
	}

	bool BeginCopyFrom(ResourceBase* r, const nosBuffer& pinData, nos::Buffer& outPinVal) override{
		Resource* res = GetResource<GPUTextureResource>(r);
		auto outputTextureDesc = static_cast<sys::vulkan::Texture*>(pinData.Data);
		auto output = vkss::DeserializeTextureInfo(outputTextureDesc);
		outPinVal = Buffer::From(vkss::ConvertTextureInfo(output));
		if (res->VkRes.Info.Texture.Height != output.Info.Texture.Height ||
			res->VkRes.Info.Texture.Width != output.Info.Texture.Width ||
			res->VkRes.Info.Texture.Format != output.Info.Texture.Format)
		{
			output.Memory = {};
			output.Info = res->VkRes.Info;
			output.Info.Texture.Usage = nosImageUsage(NOS_IMAGE_USAGE_TRANSFER_SRC | NOS_IMAGE_USAGE_TRANSFER_DST | NOS_IMAGE_USAGE_SAMPLED);

			sys::vulkan::TTexture texDef = vkss::ConvertTextureInfo(output);
			texDef.unscaled = true;
			outPinVal = Buffer::From(texDef);
			return true;
		}
		return false;
	}
};

struct GPUBufferResource : ResourceInterface {
	static constexpr ResourceType RESOURCE_TYPE = ResourceInterface::ResourceType::GPUBuffer;
	typedef nosResourceShareInfo PinData;
	struct Resource : ResourceBase
	{
		Resource(vkss::Resource bufferRes) : VkRes(std::move(bufferRes))
		{
			ResourceType = RESOURCE_TYPE;
		}
		~Resource()
		{
			if (Params.WaitEvent)
				nosVulkan->WaitGpuEvent(&Params.WaitEvent, UINT64_MAX);
		}
		vkss::Resource VkRes;
		struct {
			nosTextureFieldType FieldType = NOS_TEXTURE_FIELD_TYPE_UNKNOWN;
			nosGPUEvent WaitEvent = 0;
		} Params{};
	};
	nosTextureFieldType WantedField = NOS_TEXTURE_FIELD_TYPE_UNKNOWN;
	static constexpr nosBufferInfo SampleBuffer =
		nosBufferInfo{ .Size = 1,
					  .Alignment = 0,
					  .Usage = nosBufferUsage(NOS_BUFFER_USAGE_TRANSFER_SRC | NOS_BUFFER_USAGE_TRANSFER_DST | NOS_BUFFER_USAGE_STORAGE_BUFFER),
					  .MemoryFlags = nosMemoryFlags(NOS_MEMORY_FLAGS_DOWNLOAD | NOS_MEMORY_FLAGS_HOST_VISIBLE) };

	nosSemaphore TransferSem = 0;
	std::atomic_uint64_t SemValue = 1;

	GPUBufferResource() : ResourceInterface(RESOURCE_TYPE) {
		nosResourceShareInfo shareInfo = {};
		shareInfo.Info.Type = NOS_RESOURCE_TYPE_BUFFER;
		shareInfo.Info.Buffer = SampleBuffer;
		Sample = nos::Buffer::From(vkss::ConvertBufferInfo(shareInfo));
		nosSemaphoreCreateInfo semCreateInfo {
			.Type = NOS_SEMAPHORE_TYPE_TIMELINE,
		};
		nosVulkan->CreateSemaphore(&semCreateInfo, &TransferSem);
	}
	~GPUBufferResource()
	{
		if (TransferSem)
			nosVulkan->DestroySemaphore(&TransferSem);
	}
	rc<ResourceBase> CreateResource() override {
		nosResourceShareInfo bufInfo = vkss::ConvertToResourceInfo(*(sys::vulkan::Buffer*)Sample.Data());
		bufInfo.Memory = {};
		bufInfo.Info.Buffer.Usage = nosBufferUsage(bufInfo.Info.Buffer.Usage | NOS_BUFFER_USAGE_STORAGE_BUFFER);
		if (auto buffer = vkss::Resource::Create(bufInfo, "Buffer Ring Resource"))
			return MakeShared<Resource>(std::move(*buffer));
		return nullptr;
	}
	void Reset(ResourceBase* res) override
	{
		auto r = GetResource<GPUBufferResource>(res);
		if (r->Params.WaitEvent)
			nosVulkan->WaitGpuEvent(&r->Params.WaitEvent, UINT64_MAX);
		r->Params = {};
		r->FrameNumber = 0;
	}

	void OnPathStart() override
	{
		WantedField = NOS_TEXTURE_FIELD_TYPE_UNKNOWN;
	}

	void WaitForDownloadToEnd(ResourceBase* res, const std::string& nodeTypeName, const std::string& nodeDisplayName, nosCopyInfo* cpy) override {
		auto r = GetResource<GPUBufferResource>(res);
		if (r->Params.WaitEvent) {
			nos::util::Stopwatch sw;
			nosVulkan->WaitGpuEvent(&r->Params.WaitEvent, UINT64_MAX);

			auto elapsed = sw.Elapsed();
			nosEngine.WatchLog((nodeTypeName + " Copy From GPU Wait: " + nodeDisplayName).c_str(),
				nos::util::Stopwatch::ElapsedString(elapsed).c_str());
		}

		nosEngine.SetPinValue(cpy->ID, r->VkRes.ToPinData());
	}

	void Copy(ResourceBase* res, nosCopyInfo* cpy, uuid NodeId) override {
		auto r = GetResource<GPUBufferResource>(res);
		nosResourceShareInfo outputResource = vkss::ConvertToResourceInfo(*InterpretPinValue<sys::vulkan::Buffer>(cpy->PinData->Data));
		{
			nosCmd cmd;
			nosCmdBeginParams beginParams = { NOS_NAME("BoundedQueue"), NodeId, &cmd, NOS_CMD_QUEUE_TYPE_TRANSFER };
			nosVulkan->Begin(&beginParams);
			nosVulkan->Copy(cmd, &r->VkRes, &outputResource, 0);
			nosCmdEndParams end{ .ForceSubmit = NOS_TRUE, .OutGPUEventHandle = &r->Params.WaitEvent };
			nosVulkan->AddSignalSemaphoreToCmd(cmd, TransferSem, SemValue);
			nosVulkan->End(cmd, &end);
		}
		{
			auto cmd = vkss::BeginCmd(NOS_NAME("Wait Transfer"), NodeId);
			nosVulkan->AddWaitSemaphoreToCmd(cmd, TransferSem, SemValue++);
			nosVulkan->End(cmd, nullptr);
		}

		nosTextureFieldType outFieldType = r->VkRes.Info.Buffer.FieldType;
		auto outputBufferDesc = *static_cast<sys::vulkan::Buffer*>(cpy->PinData->Data);
		outputBufferDesc.mutate_field_type((sys::vulkan::FieldType)outFieldType);
		nosEngine.SetPinValue(cpy->ID, nos::Buffer::From(outputBufferDesc));
	}

	void* GetPinInfo(nosPinInfo& pin, bool rejectFieldMismatch) override{
		nosResourceShareInfo input = vkss::ConvertToResourceInfo(*InterpretPinValue<sys::vulkan::Buffer>(pin.Data->Data));

		nosTextureFieldType incomingField = input.Info.Buffer.FieldType;

		if (!input.Memory.Handle)
			return nullptr;


		if (rejectFieldMismatch)
		{
			if (WantedField == NOS_TEXTURE_FIELD_TYPE_UNKNOWN)
				WantedField = incomingField;

			auto outInterlaced = vkss::IsTextureFieldTypeInterlaced(WantedField);
			auto inInterlaced = vkss::IsTextureFieldTypeInterlaced(incomingField);
			if ((inInterlaced && outInterlaced) && incomingField != WantedField)
			{
				nosEngine.LogW("Field mismatch. Waiting for a new frame.");
				return nullptr;
			}
			WantedField = vkss::FlippedField(WantedField);
		}

		return pin.Data->Data;
	}

	nosResult Push(ResourceBase* r, void* pinInfo, nosNodeExecuteParams* params, nos::Name ringExecuteName, bool pushEventForCopyFrom) override {
		Resource* res = GetResource<GPUBufferResource>(r);
		res->FrameNumber = params->FrameNumber;

		nosResourceShareInfo input = vkss::ConvertToResourceInfo(*InterpretPinValue<sys::vulkan::Buffer>(pinInfo));
		res->VkRes.Info.Buffer.FieldType = input.Info.Buffer.FieldType;

		if (res->Params.WaitEvent)
		{
			nos::util::Stopwatch sw;
			nosVulkan->WaitGpuEvent(&res->Params.WaitEvent, UINT64_MAX);
			auto elapsed = sw.Elapsed();
			nosEngine.WatchLog((ringExecuteName.AsString() + " Execute GPU Wait: " + nos::Name(params->NodeName).AsString()).c_str(),
				nos::util::Stopwatch::ElapsedString(elapsed).c_str());
		}
		{
			nosCmd cmd;
			nosCmdBeginParams beginParams;
			beginParams = { ringExecuteName, params->NodeId, &cmd, NOS_CMD_QUEUE_TYPE_TRANSFER };
			nosVulkan->Begin(&beginParams);
			nosVulkan->Copy(cmd, &input, &res->VkRes, 0);
			nosVulkan->AddSignalSemaphoreToCmd(cmd, TransferSem, SemValue);
			nosCmdEndParams end{ .ForceSubmit = NOS_TRUE, .OutGPUEventHandle = pushEventForCopyFrom ? &res->Params.WaitEvent : nullptr };
			nosVulkan->End(cmd, &end);
		}
		{
			auto cmd = vkss::BeginCmd(NOS_NAME("Wait Transfer"), params->NodeId);
			nosVulkan->AddWaitSemaphoreToCmd(cmd, TransferSem, SemValue++);
			nosVulkan->End(cmd, nullptr);
		}
		return NOS_RESULT_SUCCESS;
	}
	bool CheckNewResource(nosName updateName, nosBuffer newVal, std::optional<nos::Buffer> oldVal) {
		auto sampleInfo = vkss::ConvertBufferInfo(vkss::ConvertToResourceInfo(*(sys::vulkan::Buffer*)(Sample.Data())));
		if (updateName == NOS_NAME_STATIC("Input"))
		{
			auto info = vkss::ConvertToResourceInfo(*InterpretPinValue<sys::vulkan::Buffer>(newVal.Data)).Info.Buffer;
			if (sampleInfo.size_in_bytes() == info.Size)
				return false;

			sampleInfo.mutate_size_in_bytes(info.Size);
			sampleInfo.mutate_element_type((sys::vulkan::BufferElementType)info.ElementType);
			sampleInfo.mutate_field_type((sys::vulkan::FieldType)info.FieldType);
			sampleInfo.mutate_alignment(info.Alignment);
			sampleInfo.mutate_usage(
				(sys::vulkan::BufferUsage)(NOS_BUFFER_USAGE_TRANSFER_SRC | NOS_BUFFER_USAGE_TRANSFER_DST));
			sampleInfo.mutate_memory_flags(
				(sys::vulkan::MemoryFlags)(NOS_MEMORY_FLAGS_DOWNLOAD | NOS_MEMORY_FLAGS_HOST_VISIBLE));
		}
		else if (updateName == NOS_NAME_STATIC("Alignment"))
		{
			nos::Buffer newAlignment = newVal;
			uint32_t alignment = *newAlignment.As<uint32_t>();
			if (sampleInfo.alignment() == alignment)
				return false;

			sampleInfo.mutate_alignment(alignment);
		}
		else
			return false;
		Sample = Buffer::From(sampleInfo);
		return true;
	}

	bool BeginCopyFrom(ResourceBase* r, const nosBuffer& pinData, nos::Buffer& outPinVal) override{
		Resource* res = GetResource<GPUBufferResource>(r);
		auto outputBufferDesc = *static_cast<sys::vulkan::Buffer*>(pinData.Data);
		auto output = vkss::ConvertToResourceInfo(outputBufferDesc);
		outPinVal = Buffer::From(vkss::ConvertBufferInfo(output));
		if (res->VkRes.Info.Buffer.Size != output.Info.Buffer.Size)
		{
			output.Memory = {};
			output.Info.Type = NOS_RESOURCE_TYPE_BUFFER;
			output.Info.Buffer = res->VkRes.Info.Buffer;
			outPinVal = Buffer::From(vkss::ConvertBufferInfo(output));
			return true;
		}
		return false;
	}
};

struct CPUTrivialResource : ResourceInterface {
	static constexpr ResourceType RESOURCE_TYPE = ResourceInterface::ResourceType::CPUGeneric;
	typedef nosBuffer PinData;
	struct Resource : ResourceBase
	{
		nos::Buffer data = {};
		Resource() { ResourceType = RESOURCE_TYPE; }
	};

	CPUTrivialResource() : ResourceInterface(RESOURCE_TYPE) {
		auto defaultVal = GetDefaultValueOfType(voidTypeName);
		if (defaultVal)
			Sample = defaultVal->GetBuffer();
	}
	rc<ResourceBase> CreateResource() override {
		rc<Resource> res = MakeShared<Resource>();
		res->data = Sample;
		return res;
	}
	void Reset(ResourceBase* res) override
	{
		auto r = GetResource<CPUTrivialResource>(res);
		r->FrameNumber = 0;
	}

	void WaitForDownloadToEnd(ResourceBase* res, const std::string& nodeTypeName, const std::string& nodeDisplayName, nosCopyInfo* cpy) override {
		auto r = GetResource<CPUTrivialResource>(res);
		nosEngine.SetPinValue(cpy->ID, r->data);
	}

	void Copy(ResourceBase* res, nosCopyInfo* cpy, uuid NodeId) override {
		auto r = GetResource<CPUTrivialResource>(res);
		nosEngine.SetPinValue(cpy->ID, r->data);
	}

	void* GetPinInfo(nosPinInfo& pin, bool rejectFieldMismatch) override {
		return (void*)pin.Data;
	}

	nosResult Push(ResourceBase* r, void* pinInfo, nosNodeExecuteParams* params, nos::Name ringExecuteName, bool pushEventForCopyFrom) override {
		Resource* res = GetResource<CPUTrivialResource>(r);
		res->FrameNumber = params->FrameNumber;
		res->data = *(nosBuffer*)pinInfo;

		return NOS_RESULT_SUCCESS;
	}
	bool CheckNewResource(nosName updateName, nosBuffer newVal, std::optional<nos::Buffer> oldVal) {
		Sample = newVal;
		return false;
	}

	bool BeginCopyFrom(ResourceBase* r, const nosBuffer& pinData, nos::Buffer& outPinVal) override {
		outPinVal = pinData;
		Resource* res = GetResource<CPUTrivialResource>(r);

		return true;
	}
};

struct TRing
{
	std::shared_ptr<ResourceInterface> ResInterface;
	bool RejectFieldMismatch = false;

    void Resize(uint32_t size)
    {
        Size = size;
        Write.Pool = {};
        Read.Pool = {};
        Resources.clear();
        for (uint32_t i = 0; i < size; ++i)
		{
			auto res = ResInterface->CreateResource();
			if (!res) {
				nosEngine.LogE("Failed to create resource for ring buffer.");
				Resources.clear();
				Write.Pool = {};
				Read.Pool = {};
				Exit = true;
				return;
			}
			Resources.push_back(res);
            Write.Pool.push_back(res.get());
        }
    }
    
	TRing(uint32_t ringSize, std::shared_ptr<ResourceInterface> resourceManager) : ResInterface(std::move(resourceManager))
    {
        Resize(ringSize);
    }

    struct
    {
        std::deque<ResourceInterface::ResourceBase *> Pool;
        std::mutex Mutex;
        std::condition_variable CV;
    } Write, Read;

    std::vector<rc<ResourceInterface::ResourceBase>> Resources;

    uint32_t Size = 0;
    nosVec2u Extent;
    std::atomic_bool Exit = false;
    std::atomic_bool ResetFrameCount = true;

    ~TRing()
    {
        Stop();
        Resources.clear();
    }

	bool IsResourcesValid()
	{ 
		return Resources.size();
	}

    void Stop()
    {
        {
            std::unique_lock l1(Write.Mutex);
            std::unique_lock l2(Read.Mutex);
            Exit = true;
        }
		Write.CV.notify_all();
		Read.CV.notify_all();
    }

    bool IsFull()
    {
        std::unique_lock lock(Read.Mutex);
		return Read.Pool.size() == Resources.size(); 
    }

	bool HasEmptySlots()
	{
		return EmptyFrames() != 0;
	}

	uint32_t EmptyFrames()
	{
		std::unique_lock lock(Write.Mutex);
		return Write.Pool.size();
	}

    bool IsEmpty()
    {
        std::unique_lock lock(Read.Mutex);
        return Read.Pool.empty();
    }

    uint32_t ReadyFrames()
    {
        std::unique_lock lock(Read.Mutex);
        return Read.Pool.size();
    }

    uint32_t TotalFrameCount()
    {
        std::unique_lock lock(Write.Mutex);
        return Size - Write.Pool.size();
    }

	ResourceInterface::ResourceBase*BeginPush()
    {
        std::unique_lock lock(Write.Mutex);
        while (Write.Pool.empty() && !Exit)
        {
            Write.CV.wait(lock);
        }
        if (Exit)
            return 0;
        ResourceInterface::ResourceBase* res = Write.Pool.front();
        Write.Pool.pop_front();
        return res;
    }

    void EndPush(ResourceInterface::ResourceBase* res)
    {
        {
            std::unique_lock lock(Read.Mutex);
            Read.Pool.push_back(res);
			assert(Read.Pool.size() <= Resources.size());
        }
        Read.CV.notify_one();
    }

    void CancelPush(ResourceInterface::ResourceBase* res)
	{
		{
			std::unique_lock lock(Write.Mutex);
			res->FrameNumber = 0;
			Write.Pool.push_front(res);
			assert(Write.Pool.size() <= Resources.size());
		}
		Write.CV.notify_one();
	}
	void CancelPop(ResourceInterface::ResourceBase* res)
	{
		{
			std::unique_lock lock(Read.Mutex);
			Read.Pool.push_front(res);
			assert(Read.Pool.size() <= Resources.size());
		}
		Read.CV.notify_one();
	}

	ResourceInterface::ResourceBase*BeginPop(uint64_t timeoutMilliseconds)
    {
        std::unique_lock lock(Read.Mutex);
        if (!Read.CV.wait_for(lock, std::chrono::milliseconds(timeoutMilliseconds), [this]() {return !Read.Pool.empty() || Exit; }))
            return 0;
        if (Exit)
            return 0;
        auto res = Read.Pool.front();
        Read.Pool.pop_front();
        return res;
    }

    void EndPop(ResourceInterface::ResourceBase*res)
    {
        {
            std::unique_lock lock(Write.Mutex);
            res->FrameNumber = 0;
            Write.Pool.push_back(res);
			assert(Write.Pool.size() <= Resources.size());
        }
        Write.CV.notify_one();
    }

    bool CanPop(uint64_t& frameNumber, uint32_t spare = 0)
    {
        std::unique_lock lock(Read.Mutex);
        if (Read.Pool.size() > spare)
        {
        	// TODO: Under current arch, schedule requests are sent for the node instead of pin, so this code shouldn't be needed, but check.
            // auto newFrameNumber = Read.Pool.front()->FrameNumber.load();
            // bool result = ResetFrameCount || !frameNumber || newFrameNumber > frameNumber;
            // frameNumber = newFrameNumber;
            // ResetFrameCount = false;
            return true;
        }

        return false;
    }

    bool CanPush()
    {
        std::unique_lock lock(Write.Mutex);
        return !Write.Pool.empty();
    }

	ResourceInterface::ResourceBase*TryPush()
    {
        if (CanPush())
            return BeginPush();
        return 0;
    }

	ResourceInterface::ResourceBase*TryPush(const std::chrono::milliseconds timeout)
    {
		{
            std::unique_lock lock(Write.Mutex);
		    if (Write.Pool.empty())
                Write.CV.wait_for(lock, timeout, [&]{ return CanPush(); });
		}
		return TryPush();
    }

	ResourceInterface::ResourceBase*TryPop(uint64_t& frameNumber, uint32_t spare = 0)
    {
        if (CanPop(frameNumber, spare))
            return BeginPop(20);
        return 0;
	}

    void Reset(bool fill)
    {
        auto& from = fill ? Write : Read;
		auto& to = fill ? Read : Write;
		std::unique_lock l1(Write.Mutex);
		std::unique_lock l2(Read.Mutex);
		while (!from.Pool.empty())
		{
			auto* slot = from.Pool.front();
			from.Pool.pop_front();
			ResInterface->Reset(slot);
			to.Pool.push_back(slot);
		}
    }
};

struct RingNodeBase : NodeContext
{
	enum class RingMode
	{
		CONSUME,
		FILL,
	};
	std::unique_ptr<TRing> Ring = nullptr;
	std::atomic_bool IsOutLive = false;

	// If reset, then reset the ring on path stop
	// If wait until full, do not output until ring is full & then start consuming
	enum class OnRestartType
	{
		RESET,
		WAIT_UNTIL_FULL
	} OnRestart;

	std::optional<uint32_t> RequestedRingSize = std::nullopt;
	bool NeedsRecreation = false;

	std::atomic_uint32_t SpareCount = 0;

	std::condition_variable ModeCV;
	std::mutex ModeMutex;
	std::atomic<RingMode> Mode = RingMode::CONSUME;
	std::size_t RemainingRepeatableCount = 0;

	std::atomic_bool RepeatWhenFilling = false;
	TypeInfo TypeInfo;

	void TypeResolved() {
		std::shared_ptr<ResourceInterface> resource;
		if (TypeInfo->TypeName == vulkanBufferTypeName)
			resource = std::make_unique<GPUBufferResource>();
		else if (TypeInfo->TypeName == vulkanTextureTypeName)
			resource = std::make_unique<GPUTextureResource>();
		else
			resource = std::make_unique<CPUTrivialResource>();

		Ring = std::make_unique<TRing>(1, std::move(resource));

		Ring->Stop();
		AddPinValueWatcher(NOS_NAME_STATIC("Size"), [this](nos::Buffer const& newSize, std::optional<nos::Buffer> oldVal) {
			uint32_t size = *newSize.As<uint32_t>();
			if (size == 0)
			{
				nosEngine.LogW((GetName() + " size cannot be 0").c_str());
				return;
			}
			if (Ring->Size != size && (!RequestedRingSize.has_value() || *RequestedRingSize != size))
			{
				nosPathCommand ringSizeChange{ .Event = NOS_RING_SIZE_CHANGE, .RingSize = size };
				nosEngine.SendPathCommand(PinName2Id[NOS_NAME_STATIC("Input")], ringSizeChange);
				SendPathRestart();
				RequestedRingSize = size;
				Ring->Stop();
			}
		});
		AddPinValueWatcher(NOS_NAME_STATIC("Input"), [this](nos::Buffer const& newBuf, std::optional<nos::Buffer> oldVal) {
			bool needsRecreation = Ring->ResInterface->CheckNewResource(NOS_NAME_STATIC("Input"), newBuf, oldVal);

			if (needsRecreation)
			{
				SendPathRestart();
				Ring->Stop();
				NeedsRecreation = true;
			}
		});
		AddPinValueWatcher(NOS_NAME_STATIC("Alignment"), [this](nos::Buffer const& newAlignment, std::optional<nos::Buffer> oldVal) {
			bool needsRecreation = Ring->ResInterface->CheckNewResource(NOS_NAME_STATIC("Alignment"), newAlignment, oldVal);

			if (needsRecreation)
			{
				SendPathRestart();
				Ring->Stop();
				NeedsRecreation = true;
			}
		});
		AddPinValueWatcher(NOS_NAME_STATIC("RepeatWhenFilling"), [this](nos::Buffer const& newVal, std::optional<nos::Buffer> oldVal) {
			RepeatWhenFilling = *newVal.As<bool>();
		});
	}

	RingNodeBase(OnRestartType onRestart) : OnRestart(onRestart), TypeInfo(voidTypeName) {}
	nosResult OnCreate(nosFbNodePtr node) override
	{
		nosName typeName = voidTypeName;
		if(auto* pins = node->pins())
			for (auto* pin : *pins)
				if (pin->name()->c_str() == outputPinName)
					IsOutLive = pin->live();
		for (auto& pin : Pins | std::views::values)
			if (pin.TypeName != voidTypeName && (pin.Name == outputPinName || pin.Name == inputPinName))
				typeName = pin.TypeName;
		if (typeName != voidTypeName) {
			TypeInfo = nos::TypeInfo(typeName);
			TypeResolved();
		}
		return NOS_RESULT_SUCCESS;
	}

	virtual std::string GetName() const = 0;

	void SendRingStats(std::string_view state) const
	{
		auto nodeName = NodeName.AsString();
		nosEngine.WatchLog((nodeName + " Read Size").c_str(), std::to_string(Ring->Read.Pool.size()).c_str());
		nosEngine.WatchLog((nodeName + " Write Size").c_str(), std::to_string(Ring->Write.Pool.size()).c_str());
		nosEngine.WatchLog((nodeName + " Total Frame Count").c_str(), std::to_string(Ring->TotalFrameCount()).c_str());
		nosEngine.WatchLog((nodeName + " State").c_str(), state.data());
	}

	nosResult OnResolvePinDataTypes(nosResolvePinDataTypesParams* params) override
	{
		if (TypeInfo.TypeName != voidTypeName)
			return NOS_RESULT_FAILED;

		TypeInfo = nos::TypeInfo(params->IncomingTypeName);

		for (size_t i = 0; i < params->PinCount; i++)
		{
			auto& pinInfo = params->Pins[i];
			std::string pinName = nosEngine.GetString(pinInfo.Name);
			if (pinName == "Input" || pinName == "Output")
				pinInfo.OutResolvedTypeName = TypeInfo->TypeName;
		}

		return NOS_RESULT_SUCCESS;
	}

	void OnPinUpdated(const nosPinUpdate* pinUpdate) {
		if (TypeInfo->TypeName == voidTypeName || Ring)
			return;

		TypeResolved();
	}

	nosResult ExecuteRingNode(nosNodeExecuteParams* params, bool pushEventForCopyFrom, nosName ringExecuteName, bool rejectFieldMismatch)
	{
		if (Ring->Exit || !Ring->IsResourcesValid() || !TypeInfo)
			return NOS_RESULT_FAILED;

		NodeExecuteParams pins(params);

		auto it = pins.find(inputPinName);
		assert(it != pins.end());
		auto& inputPin = it->second;
		assert(inputPin.Data);

		void* input = Ring->ResInterface->GetPinInfo(pins[inputPinName], rejectFieldMismatch);
		if (input == nullptr) {
			SendScheduleRequest(0);
			return NOS_RESULT_FAILED;
		}

		if (Ring->IsFull())
		{
			nosEngine.LogI("Trying to push while ring is full");
		}

		typename ResourceInterface::ResourceBase* slot = nullptr;
		
		SendRingStats("Pre Push");
		{
			nos::util::Stopwatch sw;
			ScopedProfilerEvent _({ .Name = "Wait For Empty Slot" });
			slot = Ring->BeginPush();
			nosEngine.WatchLog((GetName() + " Begin Push").c_str(), nos::util::Stopwatch::ElapsedString(sw.Elapsed()).c_str());
		}

		Ring->ResInterface->Push(slot, input, params, ringExecuteName, pushEventForCopyFrom);
		
		bool isFillComplete = false;
		if(Mode == RingMode::FILL)
			isFillComplete = Ring->Write.Pool.size() == 0;
		Ring->EndPush(slot);
		SendRingStats("Post Push");

		if (isFillComplete)
		{
			std::unique_lock lock(ModeMutex);
			Mode = RingMode::CONSUME;
			ModeCV.notify_all();
		}
		if (!IsOutLive)
		{
			ChangePinLiveness(NOS_NAME_STATIC("Output"), true);
			IsOutLive = true;
		}

		return NOS_RESULT_SUCCESS;
	}

	nosResult CommonCopyFrom(nosCopyInfo* cpy, ResourceInterface::ResourceBase** foundSlot) {
		if (!Ring || Ring->Exit)
			return NOS_RESULT_FAILED;

		// This is needed since out pins are created as dirty and CopyFrom is called for dirty pins.
		if (!IsOutLive)
			return NOS_RESULT_SUCCESS;

		if (OnRestart == OnRestartType::WAIT_UNTIL_FULL && RepeatWhenFilling)
		{
			if (RemainingRepeatableCount > 0)
			{
				RemainingRepeatableCount--;
				return NOS_RESULT_SUCCESS;
			}
		}
		else if (Mode == RingMode::FILL)
		{
			//Sleep for 100 ms & if still Fill, return pending
			std::unique_lock lock(ModeMutex);
			if (!ModeCV.wait_for(lock, std::chrono::milliseconds(100), [this] { return Mode != RingMode::FILL; }))
				return NOS_RESULT_PENDING;
		}

		ResourceInterface::ResourceBase* slot;
		SendRingStats("Pre Begin Pop");
		{
			ScopedProfilerEvent _({ .Name = "Wait For Filled Slot" });
			slot = Ring->BeginPop(100);
		}
		// If timeout or exit
		if (!slot)
			return Ring->Exit ? NOS_RESULT_FAILED : NOS_RESULT_PENDING;
		SendRingStats("Post Begin Pop");

		nosResourceShareInfo output;
		nos::Buffer outPinVal;
		bool changePinValue = Ring->ResInterface->BeginCopyFrom(slot, *cpy->PinData, outPinVal);
		if (changePinValue) {
			nosEngine.SetPinValueByName(NodeId, NOS_NAME_STATIC("Output"), outPinVal);
		}
		*foundSlot = slot;
		return NOS_RESULT_SUCCESS;
	}

	void OnPathCommand(const nosPathCommand* command) override
	{
		switch (command->Event)
		{
		case NOS_RING_SIZE_CHANGE: {
			if (command->RingSize == 0)
			{
				nosEngine.LogW((GetName() + " size cannot be 0.").c_str());
				return;
			}
			RequestedRingSize = command->RingSize;
			nosEngine.SetPinValue(*GetPinId(NOS_NAME("Size")), nos::Buffer::From(command->RingSize));
			break;
		}
		default: return;
		}
	}

	void SendScheduleRequest(uint32_t count, bool reset = false) const
	{
		nosScheduleNodeParams schedule{
			.NodeId = NodeId,
			.AddScheduleCount = count,
			.Reset = reset
		};
		nosEngine.ScheduleNode(&schedule);
	}

	void OnPathStop() override
	{
		if (OnRestart == OnRestartType::WAIT_UNTIL_FULL)
			Mode = RingMode::FILL;
		if (Ring)
			Ring->Stop();
	}

	void OnPathStart() override
	{
		if (!Ring) return;
		// Reset read pool for Queues(OnRestart::RESET) and Rings(OnRestart::WAIT_UNTIL_FULL) with Repeat too, if repeat, then size-1 CopyFroms will be repeated from the last buffer.
		if (OnRestart == OnRestartType::RESET || RepeatWhenFilling)
			Ring->Reset(false);
		// We must wait for at least a frame to be sure that providing path is started and running smoothly
		else if (Ring->IsFull() && !Ring->Read.Pool.empty())
		{
			Ring->Write.Pool.push_back(Ring->Read.Pool.front());
			Ring->Read.Pool.pop_front();
		}
		if (RequestedRingSize)
		{
			Ring->Resize(*RequestedRingSize);
			RequestedRingSize = std::nullopt;
			NeedsRecreation = false;
		}
		if (NeedsRecreation)
		{
			Ring = std::make_unique<TRing>(Ring->Size, Ring->ResInterface);
			NeedsRecreation = false;
		}
		if (!Ring->IsResourcesValid())
		{
			// This is here since invalid state might be solved after execution
			SendScheduleRequest(1);
			return;
		}
		auto emptySlotCount = Ring->Write.Pool.size();
		if (RepeatWhenFilling)
			RemainingRepeatableCount = std::max(emptySlotCount, (size_t)1) - 1;
		nosScheduleNodeParams schedule{ .NodeId = NodeId, .AddScheduleCount = emptySlotCount };
		nosEngine.ScheduleNode(&schedule);
		Ring->Exit = false;
		Ring->ResInterface->OnPathStart();
	}

	void SendPathRestart()
	{
		nosEngine.SendPathRestart(PinName2Id[NOS_NAME_STATIC("Input")]);
	}

	void OnEndFrame(uuid const& pinId, nosEndFrameCause cause) override
	{
		if (cause != NOS_END_FRAME_FAILED)
			return;
		if (pinId == PinName2Id[NOS_NAME_STATIC("Output")])
			return;
		if(!IsOutLive)
			return;
		ChangePinLiveness(NOS_NAME_STATIC("Output"), false);
		IsOutLive = false;
	}

	~RingNodeBase() override
	{
		if (Ring)
			Ring->Stop();
	}
};

} // namespace nos