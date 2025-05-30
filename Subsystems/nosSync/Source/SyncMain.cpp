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
	uint64_t NextEventId = 0;
	std::unordered_map<uint32_t, std::unordered_map<uint64_t, EventGroupEntry>> Groups;
	std::unordered_map<uint32_t, uint64_t> NumConsensusRequestRcvd;
} GEventSync = {};

nosResult NOSAPI_CALL RegisterEvent(
	uint32_t eventGroupId,
	void* userData,
	nosEventWaitPfn waitFn,
	uint64_t* outId)
{
	if (!waitFn || !outId)
		return NOS_RESULT_INVALID_ARGUMENT; // Invalid parameters
	std::unique_lock lock(GEventSync.Mutex);
	GEventSync.NextEventId++;
	GEventSync.Groups[eventGroupId][GEventSync.NextEventId] = {
		.Id = GEventSync.NextEventId,
		.PfnWait = waitFn,
		.UserData = userData
	};
	*outId = GEventSync.NextEventId;
	return NOS_RESULT_SUCCESS;
}

nosResult NOSAPI_CALL UnregisterEvent(uint64_t eventId)
{
	std::unique_lock lock(GEventSync.Mutex);
	for (auto& [eventGroupId, entries] : GEventSync.Groups)
	{
		for (auto it = entries.begin(); it != entries.end(); )
		{
			if (it->first == eventId)
				it = entries.erase(it);
			else
				++it;
		}
	}
	return NOS_RESULT_SUCCESS;
}

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
				auto res = entry.PfnWait(entry.UserData, &ts);
				if (res == NOS_RESULT_FAILED)
				{
					// Error when waiting for an event, consensus cannot be achieved
					return res;
				}
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
				nosEngine.LogD("Event group %lu entry %llu: Current TS %llu, waiting for %llu",
					eventGroupId, entry.Id, ts, maxTs);
				auto res = entry.PfnWait(entry.UserData, &ts);
				if (res == NOS_RESULT_FAILED)
				{
					// Error when waiting for an event, consensus cannot be achieved
					return res;
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
	subsystem->RegisterEvent = RegisterEvent;
	subsystem->UnregisterEvent = UnregisterEvent;
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
