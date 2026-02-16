// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>

#include <nosSysVulkan/nosVulkanSubsystem.h>

NOS_INIT()
NOS_VULKAN_INIT()

NOS_BEGIN_IMPORT_DEPS()
	NOS_VULKAN_IMPORT()
NOS_END_IMPORT_DEPS()

namespace nos::utilities
{
nosResult RegisterAlwaysDirty(nosNodeFunctions*);
nosResult RegisterConditionalTrigger(nosNodeFunctions*);
nosResult RegisterCPUSleep(nosNodeFunctions*);
nosResult RegisterMultiLiveOut(nosNodeFunctions*);
nosResult RegisterPrintLog(nosNodeFunctions*);
nosResult RegisterPropagateExecution(nosNodeFunctions*);
nosResult RegisterRepeatingJunction(nosNodeFunctions*);
nosResult RegisterScheduleOnRequest(nosNodeFunctions*);
nosResult RegisterScheduleRequest(nosNodeFunctions*);
nosResult RegisterShowStatusNode(nosNodeFunctions*);
nosResult RegisterSink(nosNodeFunctions*);
nosResult RegisterSwitchTrigger(nosNodeFunctions*);
nosResult RegisterSyncMultiOutlet(nosNodeFunctions*);
nosResult RegisterTriggerOnAnyInput(nosNodeFunctions*);
}

namespace nos
{
void RegisterToggleNode(nosNodeFunctions*);
}

namespace nos::flow
{
enum class Nodes : size_t
{
	AlwaysDirty,
	ConditionalTrigger,
	CPUSleep,
	MultiLiveOut,
	PrintLog,
	PropagateExecution,
	RepeatingJunction,
	ScheduleOnRequest,
	ScheduleRequest,
	ShowStatus,
	Sink,
	SwitchTrigger,
	SyncMultiOutlet,
	TriggerOnAnyInput,
	Toggle,
	Count,
};

nosResult NOSAPI_CALL ExportNodeFunctions(size_t* outCount, nosNodeFunctions** outList)
{
	if (outCount)
		*outCount = static_cast<size_t>(Nodes::Count);
	if (!outList)
		return NOS_RESULT_SUCCESS;

	for (size_t i = 0; i < static_cast<size_t>(Nodes::Count); ++i)
	{
		auto* node = outList[i];
		switch (static_cast<Nodes>(i))
		{
		case Nodes::AlwaysDirty:
			NOS_SOFT_CHECK(nos::utilities::RegisterAlwaysDirty(node) == NOS_RESULT_SUCCESS);
			break;
		case Nodes::ConditionalTrigger:
			NOS_SOFT_CHECK(nos::utilities::RegisterConditionalTrigger(node) == NOS_RESULT_SUCCESS);
			break;
		case Nodes::CPUSleep:
			NOS_SOFT_CHECK(nos::utilities::RegisterCPUSleep(node) == NOS_RESULT_SUCCESS);
			break;
		case Nodes::MultiLiveOut:
			NOS_SOFT_CHECK(nos::utilities::RegisterMultiLiveOut(node) == NOS_RESULT_SUCCESS);
			break;
		case Nodes::PrintLog:
			NOS_SOFT_CHECK(nos::utilities::RegisterPrintLog(node) == NOS_RESULT_SUCCESS);
			break;
		case Nodes::PropagateExecution:
			NOS_SOFT_CHECK(nos::utilities::RegisterPropagateExecution(node) == NOS_RESULT_SUCCESS);
			break;
		case Nodes::RepeatingJunction:
			NOS_SOFT_CHECK(nos::utilities::RegisterRepeatingJunction(node) == NOS_RESULT_SUCCESS);
			break;
		case Nodes::ScheduleOnRequest:
			NOS_SOFT_CHECK(nos::utilities::RegisterScheduleOnRequest(node) == NOS_RESULT_SUCCESS);
			break;
		case Nodes::ScheduleRequest:
			NOS_SOFT_CHECK(nos::utilities::RegisterScheduleRequest(node) == NOS_RESULT_SUCCESS);
			break;
		case Nodes::ShowStatus:
			NOS_SOFT_CHECK(nos::utilities::RegisterShowStatusNode(node) == NOS_RESULT_SUCCESS);
			break;
		case Nodes::Sink:
			NOS_SOFT_CHECK(nos::utilities::RegisterSink(node) == NOS_RESULT_SUCCESS);
			break;
		case Nodes::SwitchTrigger:
			NOS_SOFT_CHECK(nos::utilities::RegisterSwitchTrigger(node) == NOS_RESULT_SUCCESS);
			break;
		case Nodes::SyncMultiOutlet:
			NOS_SOFT_CHECK(nos::utilities::RegisterSyncMultiOutlet(node) == NOS_RESULT_SUCCESS);
			break;
		case Nodes::TriggerOnAnyInput:
			NOS_SOFT_CHECK(nos::utilities::RegisterTriggerOnAnyInput(node) == NOS_RESULT_SUCCESS);
			break;
		case Nodes::Toggle:
			nos::RegisterToggleNode(node);
			break;
		default:
			break;
		}
	}

	return NOS_RESULT_SUCCESS;
}

void GetRenamedTypes(nosName* outFrom, nosName* outTo, size_t* outSize)
{
	static std::vector<std::pair<nos::Name, nos::Name>> renames = {
		{NOS_NAME("nos.utilities.SinkMode"), NOS_NAME("nos.flow.SinkMode")},
		{NOS_NAME("zd.utilities.SinkMode"), NOS_NAME("nos.flow.SinkMode")},
	};

	if (!outFrom)
	{
		*outSize = renames.size();
		return;
	}

	for (size_t i = 0; i < renames.size(); ++i)
	{
		outFrom[i] = renames[i].first;
		outTo[i] = renames[i].second;
	}
}

void GetRenamedNodeClasses(nosName* outFrom, nosName* outTo, size_t* outSize)
{
	static std::vector<std::pair<nos::Name, nos::Name>> renames = {
		{NOS_NAME("nos.utilities.AlwaysDirty"), NOS_NAME("nos.flow.AlwaysDirty")},
		{NOS_NAME("nos.utilities.ConditionalTrigger"), NOS_NAME("nos.flow.ConditionalTrigger")},
		{NOS_NAME("nos.utilities.ExecDepend"), NOS_NAME("nos.flow.ExecDepend")},
		{NOS_NAME("nos.utilities.CPUSleep"), NOS_NAME("nos.flow.CPUSleep")},
		{NOS_NAME("nos.utilities.MultiLiveOut"), NOS_NAME("nos.flow.MultiLiveOut")},
		{NOS_NAME("nos.utilities.PrintLog"), NOS_NAME("nos.flow.PrintLog")},
		{NOS_NAME("nos.utilities.PropagateExecution"), NOS_NAME("nos.flow.PropagateExecution")},
		{NOS_NAME("nos.utilities.RepeatingJunction"), NOS_NAME("nos.flow.RepeatingJunction")},
		{NOS_NAME("nos.utilities.ScheduleOnRequest"), NOS_NAME("nos.flow.ScheduleOnRequest")},
		{NOS_NAME("nos.utilities.ScheduleRequest"), NOS_NAME("nos.flow.ScheduleRequest")},
		{NOS_NAME("nos.utilities.ShowStatus"), NOS_NAME("nos.flow.ShowStatus")},
		{NOS_NAME("nos.utilities.ShowStatusNode"), NOS_NAME("nos.flow.ShowStatus")},
		{NOS_NAME("nos.utilities.Sink"), NOS_NAME("nos.flow.Sink")},
		{NOS_NAME("nos.utilities.SinkGraph"), NOS_NAME("nos.flow.SinkGraph")},
		{NOS_NAME("nos.utilities.SwitchTrigger"), NOS_NAME("nos.flow.SwitchTrigger")},
		{NOS_NAME("nos.utilities.SyncMultiOutlet"), NOS_NAME("nos.flow.SyncMultiOutlet")},
		{NOS_NAME("nos.utilities.ThreadedSyncMultiOutlet"), NOS_NAME("nos.flow.ThreadedSyncMultiOutlet")},
		{NOS_NAME("nos.utilities.TimedFunctionSignaller"), NOS_NAME("nos.flow.TimedFunctionSignaller")},
		{NOS_NAME("nos.utilities.TriggerOnAnyInput"), NOS_NAME("nos.flow.TriggerOnAnyInput")},
		{NOS_NAME("nos.utilities.Toggle"), NOS_NAME("nos.flow.Toggle")},
		{NOS_NAME("zd.utilities.AlwaysDirty"), NOS_NAME("nos.flow.AlwaysDirty")},
		{NOS_NAME("zd.utilities.ConditionalTrigger"), NOS_NAME("nos.flow.ConditionalTrigger")},
		{NOS_NAME("zd.utilities.ExecDepend"), NOS_NAME("nos.flow.ExecDepend")},
		{NOS_NAME("zd.utilities.CPUSleep"), NOS_NAME("nos.flow.CPUSleep")},
		{NOS_NAME("zd.utilities.MultiLiveOut"), NOS_NAME("nos.flow.MultiLiveOut")},
		{NOS_NAME("zd.utilities.PrintLog"), NOS_NAME("nos.flow.PrintLog")},
		{NOS_NAME("zd.utilities.PropagateExecution"), NOS_NAME("nos.flow.PropagateExecution")},
		{NOS_NAME("zd.utilities.RepeatingJunction"), NOS_NAME("nos.flow.RepeatingJunction")},
		{NOS_NAME("zd.utilities.ScheduleOnRequest"), NOS_NAME("nos.flow.ScheduleOnRequest")},
		{NOS_NAME("zd.utilities.ScheduleRequest"), NOS_NAME("nos.flow.ScheduleRequest")},
		{NOS_NAME("zd.utilities.ShowStatus"), NOS_NAME("nos.flow.ShowStatus")},
		{NOS_NAME("zd.utilities.ShowStatusNode"), NOS_NAME("nos.flow.ShowStatus")},
		{NOS_NAME("zd.utilities.Sink"), NOS_NAME("nos.flow.Sink")},
		{NOS_NAME("zd.utilities.SinkGraph"), NOS_NAME("nos.flow.SinkGraph")},
		{NOS_NAME("zd.utilities.SwitchTrigger"), NOS_NAME("nos.flow.SwitchTrigger")},
		{NOS_NAME("zd.utilities.SyncMultiOutlet"), NOS_NAME("nos.flow.SyncMultiOutlet")},
		{NOS_NAME("zd.utilities.ThreadedSyncMultiOutlet"), NOS_NAME("nos.flow.ThreadedSyncMultiOutlet")},
		{NOS_NAME("zd.utilities.TimedFunctionSignaller"), NOS_NAME("nos.flow.TimedFunctionSignaller")},
		{NOS_NAME("zd.utilities.TriggerOnAnyInput"), NOS_NAME("nos.flow.TriggerOnAnyInput")},
		{NOS_NAME("zd.utilities.Toggle"), NOS_NAME("nos.flow.Toggle")},
	};

	if (!outFrom)
	{
		*outSize = renames.size();
		return;
	}

	for (size_t i = 0; i < renames.size(); ++i)
	{
		outFrom[i] = renames[i].first;
		outTo[i] = renames[i].second;
	}
}

extern "C"
{
NOSAPI_ATTR nosResult NOSAPI_CALL nosExportPlugin(nosPluginFunctions* out)
{
	out->ExportNodeFunctions = ExportNodeFunctions;
	out->GetRenamedTypes = GetRenamedTypes;
	out->GetRenamedNodeClasses = GetRenamedNodeClasses;
	return NOS_RESULT_SUCCESS;
}
}

} // namespace nos::flow
