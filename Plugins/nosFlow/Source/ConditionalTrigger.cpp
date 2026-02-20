#include <Nodos/Plugin.hpp>


namespace nos::flow
{
struct ConditionalTrigger : NodeContext
{
	using NodeContext::NodeContext;

	nosResult Branch(nosFunctionExecuteParams* functionExecParams)
	{
		NodeExecuteParams params(functionExecParams->FunctionNodeExecuteParams);
		bool condition = *params.GetPinData<bool>(NOS_NAME("Condition"));
		functionExecParams->MarkOutExeDirty = false;
		if (condition)
			nosEngine.SetPinDirty(params[NOS_NAME("True")].Id);
		else
			nosEngine.SetPinDirty(params[NOS_NAME("False")].Id);
		return NOS_RESULT_SUCCESS;
	}

	NOS_DECLARE_FUNCTIONS(
		NOS_ADD_FUNCTION(NOS_NAME("Branch"), Branch),
	);
};


void RegisterConditionalTrigger(nosNodeFunctions* out)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("ConditionalTrigger"), ConditionalTrigger, out);
}

}
