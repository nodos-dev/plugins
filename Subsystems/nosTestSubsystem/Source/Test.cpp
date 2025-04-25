// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#include <nosTestSubsystem/TestSubsystem.h>
#include <Nodos/PluginAPI.h>
#include <Nodos/Helpers.hpp>
NOS_INIT()

NOS_BEGIN_IMPORT_DEPS()
NOS_END_IMPORT_DEPS()

extern "C"
{

NOSAPI_ATTR nosResult NOSAPI_CALL OnRequestAPI(uint32_t minor, void** outSubsystemContext)
{
	static TestSubsystem testSubsystem = {};
	*outSubsystemContext = &testSubsystem;
	return NOS_RESULT_SUCCESS;
}

NOSAPI_ATTR nosResult NOSAPI_CALL nosExportPlugin(nosPluginFunctions* subsystemFunctions)
{
	subsystemFunctions->OnRequestAPI = OnRequestAPI;
	return NOS_RESULT_SUCCESS;
}

NOSAPI_ATTR nosResult NOSAPI_CALL nosUnloadSubsystem()
{
	return NOS_RESULT_SUCCESS;
}

}
