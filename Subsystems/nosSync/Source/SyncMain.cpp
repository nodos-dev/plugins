// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#include <nosSync/nosSync.h>

#include <Nodos/Plugin.hpp>

#include <shared_mutex>
#include <format>
#include <numeric>
#include <random>

#include "Promise.hpp"

NOS_INIT();

NOS_BEGIN_IMPORT_DEPS()
NOS_END_IMPORT_DEPS()

namespace nos::sync
{
std::unordered_map<uint32_t, nosSyncSubsystem*> GExportedSubsystemVersions;

nosResult NOSAPI_CALL RegisterEventGroup(const nosRegisterEventGroupParams* params);
nosResult NOSAPI_CALL RegisterEvent(const nosRegisterEventParams* params);
nosResult NOSAPI_CALL UnregisterEvent(uint64_t eventId);
nosResult NOSAPI_CALL UnregisterEventGroup(uint32_t eventGroupId);
nosResult NOSAPI_CALL WaitForConsensus(uint32_t eventId, uint64_t* outTimestamp, uint64_t* outCount);

nosResult NOSAPI_CALL ConstructPromiseObject(nosBuffer buffer, nosForeignHandle* outForeignHandle, nosBuffer* outSerializedData);
void NOSAPI_CALL ReleasePromiseObject(nosForeignHandle foreignHandle);
nosResult NOSAPI_CALL SerializePromiseObject(nosForeignHandle foreignHandle, nosBuffer* outBuffer);

nosResult NOSAPI_CALL Export(uint32_t minorVersion, void** outSubsystemContext)
{
	auto it = GExportedSubsystemVersions.find(minorVersion);
	if (it != GExportedSubsystemVersions.end())
	{
		*outSubsystemContext = it->second;
		return NOS_RESULT_SUCCESS;
	}
	auto* subsystem = new nosSyncSubsystem();
	subsystem->RegisterEventGroup = RegisterEventGroup;
	subsystem->RegisterEvent = RegisterEvent;
	subsystem->UnregisterEvent = UnregisterEvent;
	subsystem->WaitForConsensus = WaitForConsensus;
	subsystem->UnregisterEventGroup = UnregisterEventGroup;
	subsystem->CreatePromise = CreatePromise;
	subsystem->WaitPromise = WaitPromise;
	subsystem->FulfillPromise = FulfillPromise;
	subsystem->ResetPromise = ResetPromise;
	*outSubsystemContext = subsystem;
	GExportedSubsystemVersions[minorVersion] = subsystem;
	return NOS_RESULT_SUCCESS;
}

nosResult NOSAPI_CALL Initialize()
{
	nosRegisterEventGroupParams defaultGroup{
		.Id = NOS_SYNC_DEFAULT_EVENT_GROUP_ID,
		.Timeout = 10.0,	// Allow 10 frames for sync
		.Tolerance = 0.49f, // Allow a fraction frame time for tolerance
	};
	RegisterEventGroup(&defaultGroup);
	nosRegisterEventGroupParams noSyncGroup{
		.Id = NOS_SYNC_NO_SYNC_EVENT_GROUP_ID,
		.Timeout = 0.0,	// Don't care
		.Tolerance = 0.0f, // Don't care
	};
	RegisterEventGroup(&noSyncGroup);
	return NOS_RESULT_SUCCESS;
}

nosResult NOSAPI_CALL UnloadSubsystem()
{
	for (auto& [minorVersion, subsystem] : GExportedSubsystemVersions)
		delete subsystem;
	UnregisterEventGroup(NOS_SYNC_DEFAULT_EVENT_GROUP_ID);
	UnregisterEventGroup(NOS_SYNC_NO_SYNC_EVENT_GROUP_ID);
	return NOS_RESULT_SUCCESS;
}

nosResult NOSAPI_CALL ExportObjectTypeInfos(size_t* outCount, nosObjectTypeInfo** outList)
{
	if (outCount)
		*outCount = 1;
	if (!outList)
		return NOS_RESULT_SUCCESS;
	auto& promiseType= *outList[0];
	promiseType.TypeName = NOS_NAME("nos.sync.Promise");
	promiseType.Functions.Construct = ConstructPromiseObject;
	promiseType.Functions.Release = ReleasePromiseObject;
	return NOS_RESULT_SUCCESS;
}

enum class Nodes : size_t
{
	FulFillPromise,
	Count,
};

void RegisterFulFillPromiseNode(nosNodeFunctions* functions);

nosResult NOSAPI_CALL ExportNodeFunctions(size_t* outCount, nosNodeFunctions** outList)
{
	if (outCount)
		*outCount = static_cast<size_t>(Nodes::Count);
	if (!outList)
		return NOS_RESULT_SUCCESS;
	auto* functions = outList[static_cast<size_t>(Nodes::FulFillPromise)];
	RegisterFulFillPromiseNode(functions);
	return NOS_RESULT_SUCCESS;
}

extern "C"
{
NOSAPI_ATTR nosResult NOSAPI_CALL nosExportPlugin(nosPluginFunctions* funcs)
{
	funcs->OnRequestAPI = Export;
	funcs->Initialize = Initialize;
	funcs->OnPreUnloadPlugin = UnloadSubsystem;
	funcs->ExportObjectTypeInfos = ExportObjectTypeInfos;
	funcs->ExportNodeFunctions = ExportNodeFunctions;
	return NOS_RESULT_SUCCESS;
}
}
}
