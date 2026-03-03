// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>

namespace nos::strings
{
struct Json2PinNode : NodeContext
{
	using NodeContext::NodeContext;

	nosResult ExecuteNode(NodeExecuteParams const& params) override
	{
		auto& outPin = params[NOS_NAME("Out")];
		if (outPin.TypeName == NOS_NAME(nos::Generic::GetFullyQualifiedName()))
		{
			SetNodeStatusMessages({{{}, "Out pin is not connected to typed pin", fb::NodeStatusMessageType::FAILURE, "", 5, true, true}});
			return NOS_RESULT_FAILED;
		}
		if (auto buf = GenerateBufferFromJson(outPin.TypeName, params.GetPinValue<const char*>(NOS_NAME("Json"))))
		{
			SetPinValue(outPin.Id, *buf);
			ClearNodeStatusMessages();
		}
		else
			SetNodeStatusMessages({{{}, "Unable to generate pin value from JSON", fb::NodeStatusMessageType::FAILURE, "", 5, true, true}});
		return NOS_RESULT_SUCCESS;
	}

	void OnPinDisconnected(nos::Name pinName) override
	{
		if (pinName == NOS_NAME("Out"))
		{
			SetPinType(NOS_NAME("Out"), NOS_NAME(nos::Generic::GetFullyQualifiedName()));
		}
	}
};


nosResult RegisterJson2Pin(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("Json2Pin"), Json2PinNode, fn);
	return NOS_RESULT_SUCCESS;
}

} // namespace nos