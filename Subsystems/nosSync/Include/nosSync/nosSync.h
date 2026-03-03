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

#define NOS_SYNC_DEFAULT_EVENT_GROUP_ID 1UL
#define NOS_SYNC_NO_SYNC_EVENT_GROUP_ID 0UL
	
typedef struct nosWaitResult {
	uint64_t TimeSinceLastEventNs;
	uint64_t EventCount;
} nosWaitResult;

typedef struct nosEventGroupHealth
{
	/// Whether the event group has a mix of different synchronization reference types,
	/// which can lead to synchronization issues.
	nosBool HasMixedReferenceTypes;
	/// Whether the event group is currently unable to reach consensus among its registered
	/// events, which can indicate synchronization problems or misconfigurations.
	nosBool UnableToReachConsensus;
} nosEventGroupHealth;

typedef nosResult (*nosResetEventPfn)(void* userData);
typedef nosResult (*nosEventWaitPfn)(void* userData, nosWaitResult* outResult);
typedef void (*nosNotifyEventGroupHealthPfn)(void* userData, const nosEventGroupHealth* health);

typedef struct nosRegisterEventGroupParams {
	uint32_t Id; /// Unique identifier for the event group.
	double Timeout; /// Number of frames to wait before timing out.
	double Tolerance; /// Fraction of an event time to allow during consensus.
} nosRegisterEventGroupParams;

typedef struct nosRegisterEventParams {
	uint32_t EventGroupId; /// The event group to register the event in. If 0, it will not be synced with other 0 ID'd events.
	nosVec2u DeltaSeconds; /// Interval between events.
	void* UserData; /// User data to pass to the wait function.
	nosResetEventPfn ResetFn; /// To initialize clocks that will be used in the wait function, and reset event states.
	nosEventWaitPfn WaitFn; /// The wait function to call when waiting for consensus.
	uint64_t* OutEventId; /// Output parameter for the unique event identifier.
	nosNotifyEventGroupHealthPfn NotifyHealthFn; /// Optional callback to receive health updates of the event group.
	nosBool IsExternallySynchronized; /// Whether this event is synchronized by an external source, used for health
									  /// monitoring and diagnostics.
} nosRegisterEventParams;


typedef struct nosSyncSubsystem
{
	/// ---------------------
	/// Event Synchronization
	/// ---------------------
	/// 
	nosResult (NOSAPI_CALL* RegisterEventGroup)(const nosRegisterEventGroupParams* params);

	/// Registers an event that can be waited with `waitFn` in an event group specified by `eventGroupId`.
	/// When WaitForConsensus is called, it will poll wait functions until they agree on the same event timestamp.
	nosResult (NOSAPI_CALL* RegisterEvent)(const nosRegisterEventParams* params);

	/// Unregisters a previously registered event using its unique identifier.
	nosResult (NOSAPI_CALL* UnregisterEvent)(uint64_t eventId);

	/// Waits until all registered waiters agree on the same event timestamp,
	/// indicating they're all synchronized on the same event occurrence.
	nosResult (NOSAPI_CALL* WaitForConsensus)(uint32_t eventId, uint64_t* outTimestamp, uint64_t* outCount);

	nosResult (NOSAPI_CALL* UnregisterEventGroup)(uint32_t eventGroupId);
	/// 
	/// ---------------------
} nosSyncSubsystem;

#pragma region Helper Declarations & Macros

// Make sure these are same with nossys file.
#define NOS_SYNC_NAME "nos.sync"
#define NOS_SYNC_VERSION_MAJOR 3
#define NOS_SYNC_VERSION_MINOR 2

extern struct nosModuleInfo nosSyncPluginInfo;
extern nosSyncSubsystem* nosSync;

#define NOS_SYNC_INIT()              \
	nosModuleInfo nosSyncPluginInfo; \
	nosSyncSubsystem* nosSync = nullptr;

#define NOS_SYNC_IMPORT() NOS_IMPORT_DEP(NOS_SYNC_NAME, nosSyncPluginInfo, nosSync)

#pragma endregion

#if __cplusplus
}
#endif

#endif