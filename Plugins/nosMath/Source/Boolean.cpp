// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>
#include <tinyexpr.h>
#include <list>

namespace nos::math
{
struct AndNode : NodeContext
{
	nosResult ExecuteNode(NodeExecuteParams const& params) override
	{
		auto& A = *params.GetPinValue<bool>(NOS_NAME("A"));
		auto& B = *params.GetPinValue<bool>(NOS_NAME("B"));
		SetPinValue(NOS_NAME("AndResult"), A && B);
		return NOS_RESULT_SUCCESS;
	}
};

struct OrNode : NodeContext
{
	nosResult ExecuteNode(NodeExecuteParams const& params) override
	{
		auto& A = *params.GetPinValue<bool>(NOS_NAME("A"));
		auto& B = *params.GetPinValue<bool>(NOS_NAME("B"));
		SetPinValue(NOS_NAME("OrResult"), A || B);
		return NOS_RESULT_SUCCESS;
	}
};

struct NotNode : NodeContext
{
	nosResult ExecuteNode(NodeExecuteParams const& params) override
	{
		auto& X = *params.GetPinValue<bool>(NOS_NAME("X"));
		SetPinValue(NOS_NAME("NotX"), !X);
		return NOS_RESULT_SUCCESS;
	}
};

void RegisterAnd(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("nos.math.And"), AndNode, fn);
}

void RegisterOr(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("nos.math.Or"), OrNode, fn);
}

void RegisterNot(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("nos.math.Not"), NotNode, fn);
}
} // namespace nos::math

