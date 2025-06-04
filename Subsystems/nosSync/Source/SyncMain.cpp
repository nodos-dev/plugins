// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#include <nosSync/nosSync.h>

#include <Nodos/SubsystemAPI.h>

#include <shared_mutex>
#include <format>
#include <numeric>

NOS_INIT_WITH_MIN_REQUIRED_MINOR(0);

NOS_BEGIN_IMPORT_DEPS()
NOS_END_IMPORT_DEPS()

namespace nos::sync
{
std::unordered_map<uint32_t, nosSyncSubsystem*> GExportedSubsystemVersions;

struct WaitResult
{
	nosResult Result;
	uint64_t Timestamp;
	uint64_t Occurrences;
};

struct Event
{
	uint64_t Id;
	nosEventWaitPfn PfnWait;
	void* UserData;
	nosVec2u DeltaSeconds = { 0, 0 }; // Time units in delta-seconds (x, y) for this event
	uint64_t LastWaitedTimestamp = 0; // Last timestamp when this event was waited on
	uint64_t NumOccurrences = 0; // Number of times this event was fired
	bool HasRequestedConsensus = false; // Whether the caller has requested consensus

	double GetEventIntervalAsSeconds() const
	{
		return static_cast<double>(DeltaSeconds.x) / DeltaSeconds.y; // Convert delta-seconds to seconds
	}

	WaitResult Wait(void* userData)
	{
		WaitResult res{};
		if (PfnWait)
		{
			uint64_t ts = 0, occurrences = 0;
			res.Result = PfnWait(userData, &ts, &occurrences);
			if (res.Result == NOS_RESULT_SUCCESS)
			{
				LastWaitedTimestamp = ts;
				NumOccurrences = occurrences;
				res.Timestamp = ts;
				res.Occurrences = occurrences;
			}
		}
		return res;
	}
};

struct EventGroup
{
	uint32_t Id;
	uint64_t Timeout; // Maximum time to wait for consensus in number of time units (delta-seconds)
	double Tolerance; // Fraction of the event time to allow for consensus
	std::unordered_map<uint64_t, Event> Events;
};

struct 
{
	std::shared_mutex Mutex;
	uint64_t NextEventId = 0;
	std::unordered_map<uint32_t, EventGroup> Groups;
} GEventSync = {};

nosVec2u GetReducedDeltaSeconds(nosVec2u deltaSecs)
{
	if (deltaSecs.y == 0)
		return deltaSecs;
	auto gcd = std::gcd(deltaSecs.x, deltaSecs.y);
	return { deltaSecs.x / gcd, deltaSecs.y / gcd };
}

nosResult NOSAPI_CALL RegisterEventGroup(const nosRegisterEventGroupParams* params)
{
	if (!params)
		return NOS_RESULT_INVALID_ARGUMENT; // Invalid parameters
	auto it = GEventSync.Groups.find(params->Id);
	if (it != GEventSync.Groups.end())
		return NOS_RESULT_FAILED;
	GEventSync.Groups[params->Id] = {
		.Id = params->Id,
		.Timeout = params->Timeout,
		.Tolerance = params->Tolerance
	};
	return NOS_RESULT_SUCCESS;
}

nosResult NOSAPI_CALL RegisterEvent(const nosRegisterEventParams* params)
{
	if (!params->WaitFn || !params->OutEventId)
		return NOS_RESULT_INVALID_ARGUMENT; // Invalid parameters
	std::unique_lock lock(GEventSync.Mutex);
	auto it = GEventSync.Groups.find(params->EventGroupId);
	if (it == GEventSync.Groups.end())
		return NOS_RESULT_NOT_FOUND; // Event group not found
	auto& eventGroup = it->second;
	auto nextId = GEventSync.NextEventId++;
	eventGroup.Events[nextId] = {
		.Id = nextId,
		.PfnWait = params->WaitFn,
		.UserData = params->UserData,
		.DeltaSeconds = GetReducedDeltaSeconds(params->DeltaSeconds)
	};
	*params->OutEventId = nextId;
	return NOS_RESULT_SUCCESS;
}

nosResult NOSAPI_CALL UnregisterEvent(uint64_t eventId)
{
	std::unique_lock lock(GEventSync.Mutex);
	for (auto git = GEventSync.Groups.begin(); git != GEventSync.Groups.end(); ++git)
	{
		auto& [groupId, group] = *git;
		for (auto eit = group.Events.begin(); eit != group.Events.end(); )
		{
			if (eit->first == eventId)
				eit = group.Events.erase(eit);
			else
				++eit;
		}
	}
	return NOS_RESULT_SUCCESS;
}

nosResult NOSAPI_CALL UnregisterEventGroup(uint32_t eventGroupId)
{
	std::unique_lock lock(GEventSync.Mutex);
	auto it = GEventSync.Groups.find(eventGroupId);
	if (it == GEventSync.Groups.end())
		return NOS_RESULT_NOT_FOUND; // Event group not found
	GEventSync.Groups.erase(it);
	return NOS_RESULT_SUCCESS;
}

nosResult NOSAPI_CALL WaitForConsensus(uint32_t eventId, uint64_t* outTimestamp, uint64_t* outCount)
{
	std::unique_lock lock(GEventSync.Mutex);
	Event* event = nullptr;
	EventGroup* eventGroup = nullptr;
	for (auto git = GEventSync.Groups.begin(); git != GEventSync.Groups.end(); ++git)
	{
		auto& [groupId, group] = *git;
		auto it = group.Events.find(eventId);
		if (it != group.Events.end())
		{
			event = &it->second;
			eventGroup = &group;
			break;
		}
	}
	if (!event)
		return NOS_RESULT_NOT_FOUND; // Event not found

	// Gather the events that should be considered for consensus.
	// They should be in the same group and have the same delta-seconds.
	std::unordered_map<uint64_t, Event*> events;
	for (const auto& [groupId, group] : GEventSync.Groups)
	{
		if (group.Id != eventGroup->Id) // Only consider events in the same group
			continue;
		for (const auto& [eid, ev] : group.Events)
		{
			if (ev.DeltaSeconds.x == event->DeltaSeconds.x && ev.DeltaSeconds.y == event->DeltaSeconds.y)
				// Only consider events with the same delta-seconds
				events[eid] = const_cast<Event*>(&ev);
		}
	}

	event->HasRequestedConsensus = true;
	uint32_t rcvd = 0;
	// Check if any event has already requested consensus
	for (const auto& [eid, ev] : events)
	{
		if (ev->HasRequestedConsensus)
			rcvd++;
	}
	auto numWaiting = events.size();
	bool shouldWait = (rcvd == 1);
	bool shouldReset = (rcvd == numWaiting);
	if (shouldReset)
	{
		for (auto& [eid, ev] : events)
			ev->HasRequestedConsensus = false; // Reset the consensus request flag for all events
	}

	if (!shouldWait)
	{
		if (outTimestamp)
			*outTimestamp = event->LastWaitedTimestamp;
		if (outCount)
			*outCount = event->NumOccurrences;
		return NOS_RESULT_SUCCESS; // Already waited for consensus
	}

	char eventGroupStr[256];
	std::snprintf(eventGroupStr, sizeof(eventGroupStr), "[%lu, (%lu/%lu)]", eventGroup->Id, event->DeltaSeconds.x, event->DeltaSeconds.y);

	nosEngine.LogD("Attempting to achieve consensus on event group %s", eventGroupStr);
	
	uint32_t syncedEventOccurrences = 0;
	uint64_t lastConsensusTimestamp = 0;
	uint64_t startTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	while (true)
	{
		uint64_t currentTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		auto currDiffFromStartNs = currentTimeNs - startTimeNs;
		auto eventIntervalNs = event->GetEventIntervalAsSeconds() * 1e9;
		auto frac = currDiffFromStartNs / eventIntervalNs;
		if (frac >= (1.0 + eventGroup->Timeout))
		{
			nosEngine.LogE("Timeout waiting for consensus on event group %s", eventGroupStr);
			return NOS_RESULT_TIMEOUT; // Timeout reached
		}
		if (events.empty())
		{
			nosEngine.LogE("No one is waiting on event group %s", eventGroupStr);
			return NOS_RESULT_NOT_FOUND; // No waiters to synchronize
		}
		std::vector<std::pair<Event*, uint64_t>> eventTimestamps;
		eventTimestamps.reserve(events.size()); // Reserve space for all events

		for (const auto& [eventId, event] : events)
		{
			uint64_t ts = 0;
			auto waitRes = event->Wait(event->UserData);
			if (waitRes.Result == NOS_RESULT_FAILED)
			{
				// Error when waiting for an event, consensus cannot be achieved
				return waitRes.Result;
			}
			ts = waitRes.Timestamp;
			eventTimestamps.emplace_back(event, ts);
		}

		// Find min and max timestamps
		uint64_t minTs = UINT64_MAX, maxTs = 0;
		for (const auto& [event, ts] : eventTimestamps)
		{
			minTs = std::min(minTs, ts);
			maxTs = std::max(maxTs, ts);
		}
		
		auto diffNs = (maxTs - minTs);
		if ((diffNs / eventIntervalNs <= eventGroup->Tolerance) && lastConsensusTimestamp != maxTs)
		{
			syncedEventOccurrences++;
			lastConsensusTimestamp = maxTs;
		}

		if (syncedEventOccurrences >= 2) // For 2 consecutive checks, all events agree on the same timestamp
		{
			auto time = std::chrono::duration_cast<std::chrono::system_clock::duration>(
				std::chrono::nanoseconds(lastConsensusTimestamp));
			std::string formatted = std::format("{:%H:%M:%S}", time);
			nosEngine.LogD("Consensus achieved on event group %s with timestamp %s", eventGroupStr, formatted.c_str());
			*outTimestamp = event->LastWaitedTimestamp;
			*outCount = event->NumOccurrences;
			return NOS_RESULT_SUCCESS;
		}

		// Wait again for those who are behind
		for (auto& [event, ts] : eventTimestamps)
		{
			if (ts < maxTs)
			{
				nosEngine.LogD("Event group %s entry %llu: Current TS %llu, waiting for %llu",
					eventGroupStr, event->Id, ts, maxTs);
				auto waitRes = event->Wait(event->UserData);
				if (waitRes.Result == NOS_RESULT_FAILED)
				{
					// Error when waiting for an event, consensus cannot be achieved
					return waitRes.Result;
				}
			}
		}
	}
	nosEngine.LogE("Unable to establish consensus on event group %s", eventGroupStr);
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
	subsystem->RegisterEventGroup = RegisterEventGroup;
	subsystem->RegisterEvent = RegisterEvent;
	subsystem->UnregisterEvent = UnregisterEvent;
	subsystem->WaitForConsensus = WaitForConsensus;
	subsystem->UnregisterEventGroup = UnregisterEventGroup;
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
NOSAPI_ATTR nosResult NOSAPI_CALL nosExportSubsystem(nosSubsystemFunctions* funcs)
{
	funcs->OnRequest = Export;
	funcs->Initialize = Initialize;
	funcs->OnPreUnloadSubsystem = UnloadSubsystem;
	return NOS_RESULT_SUCCESS;
}
}
}
