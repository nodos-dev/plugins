#include <Nodos/Plugin.hpp>

namespace nos::flow
{
struct AlwaysDirtyNode : nos::NodeContext
{
	using NodeContext::NodeContext;
	void OnPinValueChanged(nos::Name pinName, uuid const& pinId, nosBuffer value) override
	{
		if (pinName == NOS_NAME_STATIC("LiveOutput"))
			ChangePinLiveness(NOS_NAME_STATIC("Output"), *InterpretObjectData<bool>(value));
	}
};

void RegisterAlwaysDirty(nosNodeFunctions* nodeFunctions)
{
	NOS_BIND_NODE_CLASS(NOS_NAME_STATIC("nos.flow.AlwaysDirty"), AlwaysDirtyNode, nodeFunctions);
}
}
