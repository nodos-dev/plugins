// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>

namespace nos::strings
{
struct IsSameStringNode : NodeContext
{
	using NodeContext::NodeContext;

	nosResult ExecuteNode(NodeExecuteParams const& params) override
	{
		auto firstStr = params.GetPinData<const char>(NOS_NAME("First"));
		auto secondStr = params.GetPinData<const char>(NOS_NAME("Second"));
		SetPinValue(NOS_NAME("IsSame"), strcmp(firstStr, secondStr) == 0);
		return NOS_RESULT_SUCCESS;
	}
};


nosResult RegisterIsSameString(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("IsSameString"), IsSameStringNode, fn);
	return NOS_RESULT_SUCCESS;
}

} // namespace nos