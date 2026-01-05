#include <Nodos/Plugin.hpp>

#include "nosSync/Sync_generated.h"
#include "Promise.hpp"

namespace nos::sync
{
struct FulFillPromiseNode : NodeContext
{
	nosResult ExecuteNode(nos::NodeExecuteParams const& params) override
	{
		auto promiseRef = params.GetPinObject<Promise>(NOS_NAME("Promise"));
		if (!promiseRef)
			return NOS_RESULT_INVALID_ARGUMENT;
		return FulfillPromise(promiseRef);
	}
};

void RegisterFulFillPromiseNode(nosNodeFunctions* functions)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("FulFillPromise"), FulFillPromiseNode, functions)
}
}