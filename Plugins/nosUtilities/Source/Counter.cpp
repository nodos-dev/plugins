// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>

namespace nos::utilities
{
NOS_REGISTER_NAME(Step);
NOS_REGISTER_NAME(Count);

// Emits a running count that advances by Step on every execution. Drive it through
// the In/Out exe pins so it can be scheduled, and call Reset to zero the count.
struct CounterNode : NodeContext
{
	using NodeContext::NodeContext;

	uint64_t Counter = 0;

	void SetCount(uint64_t value)
	{
		Counter = value;
		SetPinValue(NSN_Count, Counter);
	}

	nosResult ExecuteNode(nos::NodeExecuteParams const& execParams) override
	{
		int32_t step = *execParams.GetPinValue<int32_t>(NSN_Step);
		SetCount(Counter + step);
		return NOS_RESULT_SUCCESS;
	}

	nosResult Reset(nosFunctionExecuteParams*)
	{
		SetCount(0);
		return NOS_RESULT_SUCCESS;
	}

	NOS_DECLARE_FUNCTIONS(
		NOS_ADD_FUNCTION(NOS_NAME("Reset"), Reset),
	);
};

nosResult RegisterCounter(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("Counter"), CounterNode, fn);
	return NOS_RESULT_SUCCESS;
}

} // namespace nos::utilities
