#include <Nodos/Plugin.hpp>
#include "Names.h"

#include <utility>
#include <optional>
#include <atomic>

#include <nosTransfer/nosTransfer.h>

#include "RingBuffer.hpp"

namespace nos::reflect
{

using RingBufferNode = RingBufferNodeBase<ServeType::WaitUntilFull>;

nosResult RegisterRingBuffer(nosNodeFunctions* funcs)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("RingBuffer"), RingBufferNode, funcs)
		return NOS_RESULT_SUCCESS;
}

}
