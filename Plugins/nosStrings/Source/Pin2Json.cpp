// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>

namespace nos::strings
{
struct Pin2JsonNode : NodeContext
{
	using NodeContext::NodeContext;

	nosResult ExecuteNode(NodeExecuteParams const& params) override
	{
		auto& dataPin = params[NOS_NAME("Data")];
		if (auto outJson = GenerateJsonFromBuffer(dataPin.TypeName, *GetObjectBuffer(*dataPin.ObjectHandle)))
		{
			SetPinValue(NOS_NAME("Json"), outJson->AsBuffer());
			ClearNodeStatusMessages();
		}
		else
			SetNodeStatusMessages({{{}, "Unable to convert pin value to JSON", fb::NodeStatusMessageType::FAILURE, "", 5, true, true}});
		return NOS_RESULT_SUCCESS;
	}

	void OnPinDisconnected(nos::Name pinName) override
	{
		if (pinName == NOS_NAME("Data"))
		{
			SetPinType(NOS_NAME("Data"), NOS_NAME("nos.Generic"));
		}
	}
};


nosResult RegisterPin2Json(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("Pin2Json"), Pin2JsonNode, fn);
	return NOS_RESULT_SUCCESS;
}

} // namespace nos