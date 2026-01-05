#include <Nodos/Plugin.hpp>
#include "Names.h"

#include <utility>
#include <optional>
#include <atomic>

#include <nosTransfer/nosTransfer.h>

#include "RingBuffer.hpp"

namespace nos::reflect
{

using CopyingRingBufferNode = RingBufferNodeBase<CopyingSlot, ServeMode::WaitUntilFull>;
using ObjectRingBufferNode = RingBufferNodeBase<ObjectSlot, ServeMode::WaitUntilFull>;

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
