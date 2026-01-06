#include <Nodos/Plugin.hpp>
#include "Names.h"

#include <utility>
#include <optional>
#include <atomic>

#include <nosTransfer/nosTransfer.h>

#include "RingBuffer.hpp"

namespace nos::reflect
{

struct ObjectRingBufferNode : RingBufferNodeBase<ObjectSlot>
{
	ObjectRingBufferNode()
		: RingBufferNodeBase(RingBufferServeMode::WaitUntilFull) {}
};

struct CopyingRingBufferNode : RingBufferNodeBase<CopyingSlot>
{
	CopyingRingBufferNode()
		: RingBufferNodeBase(RingBufferServeMode::WaitUntilFull) {}
};

nosResult RegisterCopyingRingBuffer(nosNodeFunctions* funcs)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("CopyingRingBuffer"), CopyingRingBufferNode, funcs)
	return NOS_RESULT_SUCCESS;
}

nosResult RegisterObjectRingBuffer(nosNodeFunctions* funcs)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("ObjectRingBuffer"), ObjectRingBufferNode, funcs)
	return NOS_RESULT_SUCCESS;
}
}
