#include <Nodos/Plugin.hpp>

#include "RingBuffer.hpp"

namespace nos::reflect
{

using BoundedQueueNode = RingBufferNodeBase<ServeType::Immediate>;

nosResult RegisterBoundedQueue(nosNodeFunctions* funcs)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("BoundedQueue"), BoundedQueueNode, funcs)
		return NOS_RESULT_SUCCESS;
}

}