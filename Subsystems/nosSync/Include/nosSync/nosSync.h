/*
 * Copyright MediaZ Teknoloji A.S. All Rights Reserved.
 */

#ifndef NOS_SYNC_H_INCLUDED
#define NOS_SYNC_H_INCLUDED

#include <Nodos/Types.h>

#if __cplusplus
extern "C"
{
#endif

typedef nosResult (*nosEventWaitPfn)(void* userData, uint64_t* outEventTimestampNs);

typedef struct nosSyncSubsystem
{
	/// ---------------------
	/// Event Synchronization
	/// ---------------------
	/// 
	/// Registers an event that can be polled with `waitFn` in an event group specified by `eventGroupId`.
	/// When WaitForConsensus is called, it will poll wait functions until they agree on the same event timestamp.
	nosResult(NOSAPI_CALL* RegisterEvent)(uint32_t eventGroupId, void* userData, nosEventWaitPfn waitFn, uint64_t* outEventId);

	/// Unregisters a previously registered event using its unique identifier.
	nosResult (NOSAPI_CALL* UnregisterEvent)(uint64_t eventId);

	/// Waits until all registered waiters agree on the same event timestamp,
	/// indicating they're all synchronized on the same event occurrence.
	nosResult (NOSAPI_CALL* WaitForConsensus)(uint32_t eventGroupId, uint64_t wiggleRoomNs, uint64_t timeoutMs);
	/// 
	/// ---------------------
} nosSyncSubsystem;

#pragma region Helper Declarations & Macros

// Make sure these are same with nossys file.
#define NOS_SYNC_NAME "nos.sync"
#define NOS_SYNC_VERSION_MAJOR 1
#define NOS_SYNC_VERSION_MINOR 0

extern struct nosModuleInfo nosSyncSubsystemModuleInfo;
extern nosSyncSubsystem* nosSync;

#define NOS_SYNC_INIT()                       \
	nosModuleInfo nosSyncSubsystemModuleInfo; \
	nosSyncSubsystem* nosSync = nullptr;

#define NOS_SYNC_IMPORT() NOS_IMPORT_DEP(NOS_SYNC_NAME, nosSyncSubsystemModuleInfo, nosSync)

#pragma endregion

#if __cplusplus
}
#endif

#endif