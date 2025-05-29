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
	/// Registers a wait function to wait for an event identified by `eventKey`.
	/// Returns a unique `id` that can be used to unregister the callback.
	void (NOSAPI_CALL* RegisterEventWaiter)(uint32_t eventGroupId, void* userData, nosEventWaitPfn waitFn, uint64_t* outId);

	/// Unregisters a previously registered event waiter using its unique identifier.
	nosResult (NOSAPI_CALL* UnregisterEventWaiter)(uint64_t id);

	/// Waits until all registered waiters agree on the same event timestamp,
	/// indicating they're all synchronized on the same event occurrence.
	nosResult (NOSAPI_CALL* WaitForConsensus)(uint32_t eventGroupId, uint64_t wiggleRoomNs);
	/// 
	/// ---------------------
} nosSyncSubsystem;

#pragma region Helper Declarations & Macros

// Make sure these are same with nossys file.
#define NOS_SYNC_NAME "nos.sync"
#define NOS_SYNC_VERSION_MAJOR 0
#define NOS_SYNC_VERSION_MINOR 1

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