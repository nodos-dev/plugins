// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>
#include <glm/glm.hpp>
#include <Builtins_generated.h>
#include <Module_generated.h>

#include <nosSysVulkan/nosVulkanSubsystem.h>
#include <nosSync/nosSync.h>

NOS_INIT()
NOS_VULKAN_INIT()
NOS_SYNC_INIT()

NOS_BEGIN_IMPORT_DEPS()
	NOS_VULKAN_IMPORT()
	NOS_SYNC_IMPORT()
NOS_END_IMPORT_DEPS()

NOS_REGISTER_NAME(Input);
NOS_REGISTER_NAME(Output);
NOS_REGISTER_NAME(In);
NOS_REGISTER_NAME(Out);

namespace nos::memory
{

enum Memory : int
{
	AsyncDownloadBuffer = 0,
	BoundedQueue,
	Buffer2Texture,
	BufferProvider,
	CopyResource,
	DeinterlacedBoundedTextureQueue,
	DeinterlacedBufferRing,
	HostVisibleBufferCopy,
	Merge,
	Resize,
	RingBuffer,
	Texture2Buffer,
	TextureProvider,
	UploadBuffer,
	UploadBufferProvider,
	WaitGPUEvent,
	Count
};

// Forward declarations
nosResult RegisterAsyncDownloadBuffer(nosNodeFunctions*);
nosResult RegisterBoundedQueue(nosNodeFunctions*);
nosResult RegisterBuffer2Texture(nosNodeFunctions*);
nosResult RegisterBufferProvider(nosNodeFunctions*);
nosResult RegisterCopyResource(nosNodeFunctions*);
nosResult RegisterDeinterlacedBoundedTextureQueue(nosNodeFunctions*);
nosResult RegisterDeinterlacedBufferRing(nosNodeFunctions*);
nosResult RegisterHostVisibleBufferCopy(nosNodeFunctions*);
nosResult RegisterMerge(nosNodeFunctions*);
nosResult RegisterResize(nosNodeFunctions*);
nosResult RegisterRingBuffer(nosNodeFunctions*);
nosResult RegisterTexture2Buffer(nosNodeFunctions*);
nosResult RegisterTextureProvider(nosNodeFunctions*);
nosResult RegisterUploadBuffer(nosNodeFunctions*);
nosResult RegisterUploadBufferProvider(nosNodeFunctions*);
nosResult RegisterWaitGPUEvent(nosNodeFunctions*);

nosResult NOSAPI_CALL ExportNodeFunctions(size_t* outSize, nosNodeFunctions** outList)
{
	*outSize = Memory::Count;
	if (!outList)
		return NOS_RESULT_SUCCESS;

#define GEN_CASE_NODE(name)					\
	case Memory::name: {					\
		auto ret = Register##name(node);	\
		if (NOS_RESULT_SUCCESS != ret)		\
			return ret;						\
		break;								\
	}

	for (int i = 0; i < Memory::Count; ++i)
	{
		auto node = outList[i];
		switch ((Memory)i) {
		default:
			break;
			GEN_CASE_NODE(AsyncDownloadBuffer)
			GEN_CASE_NODE(BoundedQueue)
			GEN_CASE_NODE(Buffer2Texture)
			GEN_CASE_NODE(BufferProvider)
			GEN_CASE_NODE(CopyResource)
			GEN_CASE_NODE(DeinterlacedBoundedTextureQueue)
			GEN_CASE_NODE(DeinterlacedBufferRing)
			GEN_CASE_NODE(HostVisibleBufferCopy)
			GEN_CASE_NODE(Merge)
			GEN_CASE_NODE(Resize)
			GEN_CASE_NODE(RingBuffer)
			GEN_CASE_NODE(Texture2Buffer)
			GEN_CASE_NODE(TextureProvider)
			GEN_CASE_NODE(UploadBuffer)
			GEN_CASE_NODE(UploadBufferProvider)
			GEN_CASE_NODE(WaitGPUEvent)
		}
	}
	
	return NOS_RESULT_SUCCESS;
}

extern "C"
{
NOSAPI_ATTR nosResult NOSAPI_CALL nosExportPlugin(nosPluginFunctions* out)
{
	out->ExportNodeFunctions = ExportNodeFunctions;
	out->GetRenamedTypes = [](nosName* outRenamedFrom, nosName* outRenamedTo, size_t* outSize) {
		if (!outRenamedFrom)
		{
			*outSize = 2;
			return;
		}
		// Migrated from nos.utilities to nos.memory
		outRenamedFrom[0] = NOS_NAME("nos.utilities.BlendMode"); outRenamedTo[0] = NOS_NAME("nos.memory.BlendMode");
		outRenamedFrom[1] = NOS_NAME("nos.utilities.ResizeMethod"); outRenamedTo[1] = NOS_NAME("nos.memory.ResizeMethod");
	};
	return NOS_RESULT_SUCCESS;
}
}
}
