// Copyright ZeroDensity AS. All Rights Reserved.
#include "Common.h"

namespace nos
{
    NOS_REGISTER_NAME(Toggle);
	NOS_REGISTER_NAME(Value);

    struct ToggleContext : public NodeContext
    {
		static nosResult GetFunctions(size_t* outCount, nosName* pName, nosPfnNodeFunctionExecute* outFunction)
		{
			static std::pair<nos::Name, nosPfnNodeFunctionExecute> Functions[1] = {
				{ NSN_Toggle, ToggleContext::Toggle},
			};

			*outCount = sizeof(Functions) / sizeof(Functions[0]);

			if (!pName || !outFunction)
				return NOS_RESULT_SUCCESS;

			for (auto& [name, fn] : Functions)
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
			bool newValue = !*params.GetPinData<bool>(NSN_Value);
			nosEngine.SetPinValueByName(context->NodeId, NSN_Value, { &newValue, sizeof(newValue) });
			return NOS_RESULT_SUCCESS;
		}
    };

    void RegisterToggleNode(nosNodeFunctions* nodeFunctions)
    {
		NOS_BIND_NODE_CLASS(NOS_NAME_STATIC("Toggle"), ToggleContext, nodeFunctions);
    }

} // namespace nos
