// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#include <Nodos/Plugin.hpp>

namespace nos::flow
{
NOS_REGISTER_NAME(Toggle);
NOS_REGISTER_NAME(Value);

struct ToggleContext : public NodeContext
{
	static nosResult GetFunctions(size_t* outCount, nosName* pName, nosPfnNodeFunctionExecute* outFunction)
	{
		static std::pair<nos::Name, nosPfnNodeFunctionExecute> functions[1] = {
			{NSN_Toggle, ToggleContext::Toggle},
		};

		*outCount = std::size(functions);

		if (!pName || !outFunction)
			return NOS_RESULT_SUCCESS;

		for (auto& [name, fn] : functions)
		{
			*pName++ = name;
			*outFunction++ = fn;
		}

		return NOS_RESULT_SUCCESS;
	}


	static nosResult Toggle(void* ctx, nosFunctionExecuteParams* fnParams)
	{
		auto context = (ToggleContext*)ctx;
		nos::NodeExecuteParams params = fnParams->ParentNodeExecuteParams;
		bool newValue = !*params.GetPinValue<bool>(NSN_Value);
		context->SetPinValue<bool>(NSN_Value, newValue);
		return NOS_RESULT_SUCCESS;
	}
};

void RegisterToggleNode(nosNodeFunctions* nodeFunctions)
{
	NOS_BIND_NODE_CLASS(NOS_NAME_STATIC("Toggle"), ToggleContext, nodeFunctions);
}

} // namespace nos::flow