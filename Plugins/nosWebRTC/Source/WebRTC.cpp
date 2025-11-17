// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/PluginAPI.h>
#include <Builtins_generated.h>
#include <Nodos/Plugin.hpp>
#include <AppService_generated.h>
#include <AppEvents_generated.h>
#include <nosSysVulkan/nosVulkanSubsystem.h>
#include "Names.h"

#include "rtc_base/ssl_adapter.h"

NOS_INIT()
NOS_VULKAN_INIT()

NOS_BEGIN_IMPORT_DEPS()
	NOS_VULKAN_IMPORT()
NOS_END_IMPORT_DEPS()

nosResult RegisterWebRTCPlayer(nosNodeFunctions* outFunctions);
nosResult RegisterWebRTCStreamer(nosNodeFunctions* outFunctions);
void RegisterWebRTCSignalingServer(nosNodeFunctions* outFunctions);

nosResult NOSAPI_CALL ExportNodeFunctions(size_t* outCount, nosNodeFunctions** outFunctions) {
	*outCount = (size_t)(3);
	if (!outFunctions)
		return NOS_RESULT_SUCCESS;

	nosResult res = RegisterWebRTCStreamer(outFunctions[0]);
	if (res != NOS_RESULT_SUCCESS)
		return res;
	res = RegisterWebRTCPlayer(outFunctions[1]);
	if (res != NOS_RESULT_SUCCESS)
		return res;
	RegisterWebRTCSignalingServer(outFunctions[2]);

	return NOS_RESULT_SUCCESS;
}

nosResult NOSAPI_CALL Initialize()
{
	if (!rtc::InitializeSSL())
		nosEngine.LogE("Failed to initialize SSL");
	return NOS_RESULT_SUCCESS;
}

nosResult NOSAPI_CALL OnPreUnload()
{
	if (!rtc::CleanupSSL())
		nosEngine.LogE("Failed to cleanup SSL");
	return NOS_RESULT_SUCCESS;
}

extern "C"
{

NOSAPI_ATTR nosResult NOSAPI_CALL nosExportPlugin(nosPluginFunctions* outFunctions)
{
	outFunctions->ExportNodeFunctions = ExportNodeFunctions;
	outFunctions->Initialize = Initialize;
	return NOS_RESULT_SUCCESS;
}

}