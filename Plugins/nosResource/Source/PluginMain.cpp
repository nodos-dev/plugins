// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>

#include <nosSysVulkan/nosVulkanSubsystem.h>
#include <nosSync/nosSync.h>

NOS_INIT()
NOS_VULKAN_INIT()
NOS_SYNC_INIT()

NOS_BEGIN_IMPORT_DEPS()
	NOS_VULKAN_IMPORT()
	NOS_SYNC_IMPORT()
NOS_END_IMPORT_DEPS()

namespace nos::resource
{
nosResult RegisterAsyncDownloadBuffer(nosNodeFunctions*);
nosResult RegisterBoundedQueue(nosNodeFunctions*);
nosResult RegisterBuffer2Texture(nosNodeFunctions*);
nosResult RegisterBufferProvider(nosNodeFunctions*);
nosResult RegisterCopyResource(nosNodeFunctions*);
nosResult RegisterDeinterlacedBoundedTextureQueue(nosNodeFunctions*);
nosResult RegisterDeinterlacedBufferRing(nosNodeFunctions*);
nosResult RegisterHostVisibleBufferCopy(nosNodeFunctions*);
nosResult RegisterRingBuffer(nosNodeFunctions*);
nosResult RegisterTexture2Buffer(nosNodeFunctions*);
nosResult RegisterTextureProvider(nosNodeFunctions*);
nosResult RegisterUploadBuffer(nosNodeFunctions*);
nosResult RegisterUploadBufferProvider(nosNodeFunctions*);
}

namespace nos::resource
{
enum class Nodes : size_t
{
	AsyncDownloadBuffer,
	BoundedQueue,
	Buffer2Texture,
	BufferProvider,
	CopyResource,
	DeinterlacedBoundedTextureQueue,
	DeinterlacedBufferRing,
	HostVisibleBufferCopy,
	RingBuffer,
	Texture2Buffer,
	TextureProvider,
	UploadBuffer,
	UploadBufferProvider,
	Count,
};

nosResult NOSAPI_CALL ExportNodeFunctions(size_t* outCount, nosNodeFunctions** outList)
{
	if (outCount)
		*outCount = static_cast<size_t>(Nodes::Count);
	if (!outList)
		return NOS_RESULT_SUCCESS;

#define GEN_CASE_NODE(name)            \
	case Nodes::name: {                \
		auto ret = Register##name(node); \
		if (ret != NOS_RESULT_SUCCESS)  \
			return ret;                 \
		break;                          \
	}

	for (size_t i = 0; i < static_cast<size_t>(Nodes::Count); ++i)
	{
		auto* node = outList[i];
		switch (static_cast<Nodes>(i))
		{
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
			GEN_CASE_NODE(RingBuffer)
			GEN_CASE_NODE(Texture2Buffer)
			GEN_CASE_NODE(TextureProvider)
			GEN_CASE_NODE(UploadBuffer)
			GEN_CASE_NODE(UploadBufferProvider)
		}
	}

#undef GEN_CASE_NODE

	return NOS_RESULT_SUCCESS;
}

void GetRenamedTypes(nosName* outFrom, nosName* outTo, size_t* outSize)
{
	static std::vector<std::pair<nos::Name, nos::Name>> renames = {
		{NOS_NAME("nos.utilities.BufferSyncTuple"), NOS_NAME("nos.resource.BufferSyncTuple")},
		{NOS_NAME("nos.utilities.TextureSyncTuple"), NOS_NAME("nos.resource.TextureSyncTuple")},
		{NOS_NAME("nos.utilities.AudioSyncTuple"), NOS_NAME("nos.resource.AudioSyncTuple")},
		{NOS_NAME("nos.utilities.VideoFrame"), NOS_NAME("nos.resource.VideoFrame")},
		{NOS_NAME("nos.utilities.AudioTexturePair"), NOS_NAME("nos.resource.AudioTexturePair")},
		{NOS_NAME("nos.utilities.FrameBufferWithAudioSync"), NOS_NAME("nos.resource.FrameBufferWithAudioSync")},
		{NOS_NAME("nos.utilities.VideoFrameWithAudioSync"), NOS_NAME("nos.resource.VideoFrameWithAudioSync")},
		{NOS_NAME("zd.utilities.BufferSyncTuple"), NOS_NAME("nos.resource.BufferSyncTuple")},
		{NOS_NAME("zd.utilities.TextureSyncTuple"), NOS_NAME("nos.resource.TextureSyncTuple")},
		{NOS_NAME("zd.utilities.AudioSyncTuple"), NOS_NAME("nos.resource.AudioSyncTuple")},
		{NOS_NAME("zd.utilities.VideoFrame"), NOS_NAME("nos.resource.VideoFrame")},
		{NOS_NAME("zd.utilities.AudioTexturePair"), NOS_NAME("nos.resource.AudioTexturePair")},
		{NOS_NAME("zd.utilities.FrameBufferWithAudioSync"), NOS_NAME("nos.resource.FrameBufferWithAudioSync")},
		{NOS_NAME("zd.utilities.VideoFrameWithAudioSync"), NOS_NAME("nos.resource.VideoFrameWithAudioSync")},
	};

	if (!outFrom)
	{
		*outSize = renames.size();
		return;
	}

	for (size_t i = 0; i < renames.size(); ++i)
	{
		outFrom[i] = renames[i].first;
		outTo[i] = renames[i].second;
	}
}

void GetRenamedNodeClasses(nosName* outFrom, nosName* outTo, size_t* outSize)
{
	static std::vector<std::pair<nos::Name, nos::Name>> renames = {
		{NOS_NAME("nos.utilities.AsyncDownloadBuffer"), NOS_NAME("nos.resource.AsyncDownloadBuffer")},
		{NOS_NAME("nos.utilities.BoundedQueue"), NOS_NAME("nos.resource.BoundedQueue")},
		{NOS_NAME("nos.utilities.BoundedTextureQueue"), NOS_NAME("nos.resource.BoundedTextureQueue")},
		{NOS_NAME("nos.utilities.Buffer2Texture"), NOS_NAME("nos.resource.Buffer2Texture")},
		{NOS_NAME("nos.utilities.BufferProvider"), NOS_NAME("nos.resource.BufferProvider")},
		{NOS_NAME("nos.utilities.CalculateDispatchSize"), NOS_NAME("nos.resource.CalculateDispatchSize")},
		{NOS_NAME("nos.utilities.CopyResource"), NOS_NAME("nos.resource.CopyResource")},
		{NOS_NAME("nos.utilities.DeinterlacedBoundedTextureQueue"), NOS_NAME("nos.resource.DeinterlacedBoundedTextureQueue")},
		{NOS_NAME("nos.utilities.DeinterlacedBufferRing"), NOS_NAME("nos.resource.DeinterlacedBufferRing")},
		{NOS_NAME("nos.utilities.HostVisibleBufferCopy"), NOS_NAME("nos.resource.HostVisibleBufferCopy")},
		{NOS_NAME("nos.utilities.RingBuffer"), NOS_NAME("nos.resource.RingBuffer")},
		{NOS_NAME("nos.utilities.Texture2Buffer"), NOS_NAME("nos.resource.Texture2Buffer")},
		{NOS_NAME("nos.utilities.TextureProvider"), NOS_NAME("nos.resource.TextureProvider")},
		{NOS_NAME("nos.utilities.UploadBuffer"), NOS_NAME("nos.resource.UploadBuffer")},
		{NOS_NAME("nos.utilities.UploadBufferProvider"), NOS_NAME("nos.resource.UploadBufferProvider")},
		{NOS_NAME("zd.utilities.AsyncDownloadBuffer"), NOS_NAME("nos.resource.AsyncDownloadBuffer")},
		{NOS_NAME("zd.utilities.BoundedQueue"), NOS_NAME("nos.resource.BoundedQueue")},
		{NOS_NAME("zd.utilities.BoundedTextureQueue"), NOS_NAME("nos.resource.BoundedTextureQueue")},
		{NOS_NAME("zd.utilities.Buffer2Texture"), NOS_NAME("nos.resource.Buffer2Texture")},
		{NOS_NAME("zd.utilities.BufferProvider"), NOS_NAME("nos.resource.BufferProvider")},
		{NOS_NAME("zd.utilities.CalculateDispatchSize"), NOS_NAME("nos.resource.CalculateDispatchSize")},
		{NOS_NAME("zd.utilities.CopyResource"), NOS_NAME("nos.resource.CopyResource")},
		{NOS_NAME("zd.utilities.DeinterlacedBoundedTextureQueue"), NOS_NAME("nos.resource.DeinterlacedBoundedTextureQueue")},
		{NOS_NAME("zd.utilities.DeinterlacedBufferRing"), NOS_NAME("nos.resource.DeinterlacedBufferRing")},
		{NOS_NAME("zd.utilities.HostVisibleBufferCopy"), NOS_NAME("nos.resource.HostVisibleBufferCopy")},
		{NOS_NAME("zd.utilities.RingBuffer"), NOS_NAME("nos.resource.RingBuffer")},
		{NOS_NAME("zd.utilities.Texture2Buffer"), NOS_NAME("nos.resource.Texture2Buffer")},
		{NOS_NAME("zd.utilities.TextureProvider"), NOS_NAME("nos.resource.TextureProvider")},
		{NOS_NAME("zd.utilities.UploadBuffer"), NOS_NAME("nos.resource.UploadBuffer")},
		{NOS_NAME("zd.utilities.UploadBufferProvider"), NOS_NAME("nos.resource.UploadBufferProvider")}
	};

	if (!outFrom)
	{
		*outSize = renames.size();
		return;
	}

	for (size_t i = 0; i < renames.size(); ++i)
	{
		outFrom[i] = renames[i].first;
		outTo[i] = renames[i].second;
	}
}

extern "C"
{
NOSAPI_ATTR nosResult NOSAPI_CALL nosExportPlugin(nosPluginFunctions* out)
{
	out->ExportNodeFunctions = ExportNodeFunctions;
	out->GetRenamedTypes = GetRenamedTypes;
	out->GetRenamedNodeClasses = GetRenamedNodeClasses;
	return NOS_RESULT_SUCCESS;
}
}

} // namespace nos::resource
