#include <Nodos/Plugin.hpp>

#include "RingBuffer.hpp"

namespace nos::reflect
{

using CopyingBoundedQueueNode = RingBufferNodeBase<CopyingSlot, ServeMode::Immediate>;
using BoundedObjectQueueNode = RingBufferNodeBase<ObjectSlot, ServeMode::Immediate>;

nosResult RegisterCopyingBoundedQueue(nosNodeFunctions* funcs)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("CopyingBoundedQueue"), CopyingBoundedQueueNode, funcs)
	return NOS_RESULT_SUCCESS;
}

nosResult RegisterBoundedObjectQueue(nosNodeFunctions* funcs)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("BoundedObjectQueue"), BoundedObjectQueueNode, funcs)
	return NOS_RESULT_SUCCESS;
}

}