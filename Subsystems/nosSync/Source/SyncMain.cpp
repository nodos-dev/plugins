// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#include <nosSync/nosSync.h>

#include <Nodos/SubsystemAPI.h>

#include <shared_mutex>

NOS_INIT_WITH_MIN_REQUIRED_MINOR(0);

NOS_BEGIN_IMPORT_DEPS()
NOS_END_IMPORT_DEPS()

namespace nos::sync
{
std::unordered_map<uint32_t, nosSyncSubsystem*> GExportedSubsystemVersions;

struct EventGroupEntry
{
	uint64_t Id;
	nosEventWaitPfn PfnWait;
	void* UserData;
};

struct 
{
	std::shared_mutex Mutex;
	uint64_t NextWaiterId = 0;
	std::unordered_map<uint32_t, std::unordered_map<uint64_t, EventGroupEntry>> Groups;
	std::unordered_map<uint32_t, uint64_t> NumConsensusRequestRcvd;
} GEventSync = {};

/// Registers a callback to wait for an event identified by `eventKey`.
/// Returns a unique `waiterId` that can be used to unregister the callback.
void NOSAPI_CALL RegisterEventWaiter(
	uint32_t eventGroupId,
	void* userData,
	nosEventWaitPfn waitFn,
	uint64_t* outId)
{
	std::unique_lock lock(GEventSync.Mutex);
	GEventSync.NextWaiterId++;
	GEventSync.Groups[eventGroupId][GEventSync.NextWaiterId] = {
		.Id = GEventSync.NextWaiterId,
		.PfnWait = waitFn,
		.UserData = userData
	};
	*outId = GEventSync.NextWaiterId;
}

/// Unregisters a previously registered event waiter using its unique identifier.
nosResult NOSAPI_CALL UnregisterEventWaiter(uint64_t waiterId)
{
	std::unique_lock lock(GEventSync.Mutex);
	for (auto& [eventGroupId, entries] : GEventSync.Groups)
	{
		for (auto it = entries.begin(); it != entries.end(); )
		{
			if (it->first == waiterId)
				it = entries.erase(it);
			else
				++it;
		}
	}
	return NOS_RESULT_SUCCESS;
}

/// Waits until all registered waiters agree on the same event timestamp,
/// indicating they're all synchronized on the same event occurrence.
/// If outEventTimestampNs returned by nosEventWaitPfn is behind the others, it should be waited again. 
nosResult NOSAPI_CALL WaitForConsensus(uint32_t eventGroupId, uint64_t wiggleRoomNs)
{
	{
		std::unique_lock lock(GEventSync.Mutex);
		auto it = GEventSync.NumConsensusRequestRcvd.find(eventGroupId);
		if (it == GEventSync.NumConsensusRequestRcvd.end())
			it = GEventSync.NumConsensusRequestRcvd.emplace(eventGroupId, 0).first;
		auto& rcvd = it->second;
		++rcvd;
		auto numWaiting = GEventSync.Groups[eventGroupId].size();
		if (numWaiting > 1)
		{
			if (rcvd > numWaiting)
				rcvd = 0;
			else
				return NOS_RESULT_SUCCESS; // Already waited on this.
		}
		else
		{
			rcvd = 0;
		}
	}

	nosEngine.LogD("Attempting to achieve consensus on event group %lu", eventGroupId);
	
	int maxAttempts = 3;
	while (maxAttempts-- > 0)
	{
		std::vector<std::pair<EventGroupEntry, uint64_t>> waiterTimestamps;
		{
			std::shared_lock lock(GEventSync.Mutex);
			auto it = GEventSync.Groups.find(eventGroupId);
			if (it == GEventSync.Groups.end() || it->second.empty())
			{
				nosEngine.LogE("No one is waiting on event group %lu", eventGroupId);
				return NOS_RESULT_NOT_FOUND; // No waiters to synchronize
			}

			for (const auto& [waiterId, entry] : it->second)
			{
				uint64_t ts = 0;
				if (entry.PfnWait)
					entry.PfnWait(entry.UserData, &ts);
				waiterTimestamps.emplace_back(entry, ts);
			}
		}

		// Find min and max timestamps
		uint64_t minTs = UINT64_MAX, maxTs = 0;
		for (const auto& [entry, ts] : waiterTimestamps)
		{
			if (ts < minTs) minTs = ts;
			if (ts > maxTs) maxTs = ts;
		}
		
		auto diff = maxTs - minTs;
		if (diff <= wiggleRoomNs)
		{
			nosEngine.LogD("Consensus achieved on event group %lu", eventGroupId);
			return NOS_RESULT_SUCCESS;
		}

		// Wait again for those who are behind
		for (auto& [entry, ts] : waiterTimestamps)
		{
			if (ts < maxTs)
			{
				if (entry.PfnWait)
				{
					nosEngine.LogD("Event group %lu entry %llu: Current TS %llu, waiting for %llu",
						eventGroupId, entry.Id, ts, maxTs);
					entry.PfnWait(entry.UserData, &ts);
				}
			}
		}
	}
	nosEngine.LogE("Unable to establish consensus on event group %lu", eventGroupId);
	return NOS_RESULT_FAILED;
}

nosResult NOSAPI_CALL Export(uint32_t minorVersion, void** outSubsystemContext)
{
	auto it = GExportedSubsystemVersions.find(minorVersion);
	if (it != GExportedSubsystemVersions.end())
	{
		*outSubsystemContext = it->second;
		return NOS_RESULT_SUCCESS;
	}
	auto* subsystem = new nosSyncSubsystem();
	subsystem->RegisterEventWaiter = RegisterEventWaiter;
	subsystem->UnregisterEventWaiter = UnregisterEventWaiter;
	subsystem->WaitForConsensus = WaitForConsensus;
	*outSubsystemContext = subsystem;
	GExportedSubsystemVersions[minorVersion] = subsystem;
	return NOS_RESULT_SUCCESS;
}

nosResult NOSAPI_CALL Initialize()
{
	return NOS_RESULT_SUCCESS;
}

nosResult NOSAPI_CALL UnloadSubsystem()
{
	for (auto& [minorVersion, subsystem] : GExportedSubsystemVersions)
		delete subsystem;
	return NOS_RESULT_SUCCESS;
}

extern "C"
{
NOSAPI_ATTR nosResult NOSAPI_CALL nosExportSubsystem(nosSubsystemFunctions* subsystemFunctions)
{
	subsystemFunctions->OnRequest = Export;
	subsystemFunctions->Initialize = Initialize;
	subsystemFunctions->OnPreUnloadSubsystem = UnloadSubsystem;
	return NOS_RESULT_SUCCESS;
}
}
}
