// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>
#include <glm/glm.hpp>
#include <Builtins_generated.h>
#include <Module_generated.h>

#include <nosSysVulkan/nosVulkanSubsystem.h>
#include <nosSync/nosSync.h>

NOS_INIT()
NOS_VULKAN_INIT()
NOS_SYNC_INIT()

NOS_BEGIN_IMPORT_DEPS()
	NOS_VULKAN_IMPORT()
	NOS_SYNC_IMPORT()
NOS_END_IMPORT_DEPS()

NOS_REGISTER_NAME(Input);
NOS_REGISTER_NAME(Output);
NOS_REGISTER_NAME(In);
NOS_REGISTER_NAME(Out);

namespace nos::execution
{

enum Execution : int
{
	AlwaysDirty = 0,
	CPUSleep,
	ConditionalTrigger,
	Host,
	MultiLiveOut,
	PrintLog,
	PropagateExecution,
	RepeatingJunction,
	ScheduleOnRequest,
	ScheduleRequest,
	ShowStatusNode,
	Sink,
	SyncMultiOutlet,
	SwitchTrigger,
	Time,
	TriggerOnAnyInput,
	Count
};

// Forward declarations
nosResult RegisterAlwaysDirty(nosNodeFunctions*);
nosResult RegisterCPUSleep(nosNodeFunctions*);
nosResult RegisterConditionalTrigger(nosNodeFunctions*);
nosResult RegisterHost(nosNodeFunctions*);
nosResult RegisterMultiLiveOut(nosNodeFunctions*);
nosResult RegisterPrintLog(nosNodeFunctions*);
nosResult RegisterPropagateExecution(nosNodeFunctions*);
nosResult RegisterRepeatingJunction(nosNodeFunctions*);
nosResult RegisterScheduleOnRequest(nosNodeFunctions*);
nosResult RegisterScheduleRequest(nosNodeFunctions*);
nosResult RegisterShowStatusNode(nosNodeFunctions*);
nosResult RegisterSink(nosNodeFunctions*);
nosResult RegisterSyncMultiOutlet(nosNodeFunctions*);
nosResult RegisterSwitchTrigger(nosNodeFunctions*);
nosResult RegisterTime(nosNodeFunctions*);
nosResult RegisterTriggerOnAnyInput(nosNodeFunctions*);

nosResult NOSAPI_CALL ExportNodeFunctions(size_t* outSize, nosNodeFunctions** outList)
{
	*outSize = Execution::Count;
	if (!outList)
		return NOS_RESULT_SUCCESS;

#define GEN_CASE_NODE(name)					\
	case Execution::name: {					\
		auto ret = Register##name(node);	\
		if (NOS_RESULT_SUCCESS != ret)		\
			return ret;						\
		break;								\
	}

	for (int i = 0; i < Execution::Count; ++i)
	{
		auto node = outList[i];
		switch ((Execution)i) {
		default:
			break;
			GEN_CASE_NODE(AlwaysDirty)
			GEN_CASE_NODE(CPUSleep)
			GEN_CASE_NODE(ConditionalTrigger)
			GEN_CASE_NODE(Host)
			GEN_CASE_NODE(MultiLiveOut)
			GEN_CASE_NODE(PrintLog)
			GEN_CASE_NODE(PropagateExecution)
			GEN_CASE_NODE(RepeatingJunction)
			GEN_CASE_NODE(ScheduleOnRequest)
			GEN_CASE_NODE(ScheduleRequest)
			GEN_CASE_NODE(ShowStatusNode)
			GEN_CASE_NODE(Sink)
			GEN_CASE_NODE(SyncMultiOutlet)
			GEN_CASE_NODE(SwitchTrigger)
			GEN_CASE_NODE(Time)
			GEN_CASE_NODE(TriggerOnAnyInput)
		}
	}
	
	return NOS_RESULT_SUCCESS;
}

extern "C"
{
NOSAPI_ATTR nosResult NOSAPI_CALL nosExportPlugin(nosPluginFunctions* out)
{
	out->ExportNodeFunctions = ExportNodeFunctions;
	return NOS_RESULT_SUCCESS;
}
}
}
