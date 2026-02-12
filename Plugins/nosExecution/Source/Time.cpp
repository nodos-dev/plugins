// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>

namespace nos::execution
{
NOS_REGISTER_NAME(Seconds);
NOS_REGISTER_NAME_SPACED(Nos_Utilities_Time, "nos.utilities.Time")
struct TimeNodeContext : NodeContext
{
	nosResult ExecuteNode(NodeExecuteParams const& params) override
	{
		float time = params.GetTotalTime(FrameCount);
		SetPinValue(NOS_NAME("Seconds"), time);
		FrameCount++;
		return NOS_RESULT_SUCCESS;
	}

	uint64_t FrameCount = 0;
};


nosResult RegisterTime(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NSN_Nos_Utilities_Time, TimeNodeContext, fn);
	return NOS_RESULT_SUCCESS;
}

} // namespace nos