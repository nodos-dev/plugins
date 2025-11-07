// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#include <nosSync/nosSync.h>

#include <Nodos/PluginAPI.h>

#include <shared_mutex>
#include <format>
#include <numeric>
#include <random>

#define RANDOMIZE_EVENT_ORDER 0 // For diagnosis, set to 1 to randomize the order of events when waiting for consensus

NOS_INIT();

NOS_BEGIN_IMPORT_DEPS()
NOS_END_IMPORT_DEPS()

namespace nos::sync
{
std::unordered_map<uint32_t, nosSyncSubsystem*> GExportedSubsystemVersions;

uint64_t NowNs()
{
	return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

struct WaitResult
{
	nosResult Result;
	uint64_t Timestamp;
	uint64_t Occurrences;
};

struct Event
{
	uint64_t Id;
	uint64_t PathGroupId = 0;
	nosResetEventPfn PfnReset = nullptr; // Optional callback for resetting the event
	nosEventWaitPfn PfnWait;
	void* UserData;
	nosVec2u DeltaSeconds = { 0, 0 }; // Time units in delta-seconds (x, y) for this event
	uint64_t LastWaitedTimestamp = 0; // Last timestamp when this event was waited on
	uint64_t NumOccurrences = 0; // Number of times this event was fired
	bool HasRequestedConsensus = false; // Whether the caller has requested consensus

	WaitResult Wait(void* userData)
	{
		WaitResult res{};
		if (PfnWait)
		{
			nosWaitResult outRes{};
			res.Result = PfnWait(userData, &outRes);
			auto nowNs = NowNs();
			if (res.Result == NOS_RESULT_SUCCESS)
			{
				auto ts = nowNs - outRes.TimeSinceLastEventNs; // Timestamp when the event was last waited on
				LastWaitedTimestamp = ts;
				NumOccurrences = outRes.EventCount;
				res.Timestamp = ts;
				res.Occurrences = NumOccurrences;
			}
		}
		return res;
	}

	nosResult Reset(void* userData)
	{
		if (PfnReset)
			return PfnReset(userData);
		return NOS_RESULT_SUCCESS;
	}
};

struct EventGroup
{
	uint32_t Id;
	double Timeout; // Maximum time to wait for consensus in number of time units (delta-seconds)
	double Tolerance; // Fraction of the event time to allow for consensus
	std::unordered_map<uint64_t, Event> Events;
};

struct 
{
	std::shared_mutex Mutex;
	uint64_t NextEventId = 1;
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

std::optional<uint64_t> GetCurrentPathGroupId()
{
	uint64_t pathGroupId = 0;
	auto res = nosEngine.GetCurrentPathGroupId(&pathGroupId);
	if (res != NOS_RESULT_SUCCESS || pathGroupId == 0)
	{
		nosEngine.LogE("Failed to get current path group ID: Error code %d", res);
		return std::nullopt;
	}
	return pathGroupId;
}

nosResult NOSAPI_CALL RegisterEvent(const nosRegisterEventParams* params)
{
	if (!params->WaitFn || !params->OutEventId)
		return NOS_RESULT_INVALID_ARGUMENT; // Invalid parameters
	std::unique_lock lock(GEventSync.Mutex);
	auto it = GEventSync.Groups.find(params->EventGroupId);
	if (it == GEventSync.Groups.end())
		return NOS_RESULT_NOT_FOUND; // Event group not found
	auto pathGroupId = GetCurrentPathGroupId();
	if (!pathGroupId.has_value())
		return NOS_RESULT_FAILED; // Failed to get current path group ID
	auto& eventGroup = it->second;
	auto nextId = GEventSync.NextEventId++;
	eventGroup.Events[nextId] = {.Id = nextId, .PathGroupId = *pathGroupId, .PfnReset = params->ResetFn,
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

bool CanTimeStepsAlign(nosVec2u deltaSec1, nosVec2u deltaSec2)
{
	if (deltaSec1.y == 0 || deltaSec2.y == 0)
		return false; // Cannot sync if any of the delta-seconds is zero
	auto gcdx = std::gcd(deltaSec1.x, deltaSec2.x);
	auto gcdy = std::gcd(deltaSec1.y, deltaSec2.y);
	return (gcdx == deltaSec1.x || gcdx == deltaSec2.x) && (gcdy == deltaSec1.y || gcdy == deltaSec2.y);
}

nosVec2u GetSmallerDeltaSeconds(nosVec2u deltaSec1, nosVec2u deltaSec2)
{
	double frac1 = static_cast<double>(deltaSec1.x) / deltaSec1.y;
	double frac2 = static_cast<double>(deltaSec2.x) / deltaSec2.y;
	return (frac1 < frac2) ? deltaSec1 : deltaSec2; // Return the smaller delta-seconds based on the fraction
}

double GetIntervalFromDeltaSecs(nosVec2u deltaSecs)
{
	if (deltaSecs.y == 0)
		return 0.0;										   // Avoid division by zero
	return static_cast<double>(deltaSecs.x) / deltaSecs.y; // Convert delta-seconds to seconds
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
	std::unordered_map<uint64_t, Ref<Event>> events;
	nosVec2u smallestDeltaSecs = event->DeltaSeconds;
	for (auto& [groupId, group] : GEventSync.Groups)
	{
		if (group.Id != eventGroup->Id) // Only consider events in the same group
			continue;
		for (auto& [eid, e] : group.Events)
		{
			if (group.Id == NOS_SYNC_NO_SYNC_EVENT_GROUP_ID)
			{
				if (e.Id != event->Id)
					continue;
				else
					events.emplace(eid, e);
			}
			else if (CanTimeStepsAlign(e.DeltaSeconds, event->DeltaSeconds)
					 && e.PathGroupId == event->PathGroupId)
			{
				// Only consider events with the same delta-seconds and in same connected component
				events.emplace(eid, e);
				smallestDeltaSecs = GetSmallerDeltaSeconds(smallestDeltaSecs, e.DeltaSeconds);
			}
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
	std::snprintf(eventGroupStr, sizeof(eventGroupStr), "[%llu:%lu:(%lu/%lu)]", event->PathGroupId, eventGroup->Id, smallestDeltaSecs.x, smallestDeltaSecs.y);

	// Diagnosis code, randomize the order of events to treat everyone equally
#if RANDOMIZE_EVENT_ORDER
	{
		std::unordered_set<size_t> usedIndices;
		std::vector<uint64_t> eventIds;
		for (const auto& [eid, ev] : events)
		{
			eventIds.push_back(eid);
		}
		
		auto copy = std::move(events);
		events = {};

		// Randomly select from eventIds, avoiding duplicates
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<size_t> dis(0, eventIds.size() - 1);
		while (usedIndices.size() < copy.size())
		{
			size_t idx = dis(gen);
			if (usedIndices.find(idx) == usedIndices.end())
			{
				usedIndices.insert(idx);
				events.emplace(eventIds[idx], copy.at(eventIds[idx]));
			}
		}
	}
	std::string idOrder = "[";
	for (auto& [id, _] : events)
	{
		idOrder += std::to_string(id) + ", ";
	}
	nosEngine.LogI("Waiting for consensus on event group %u with events: %s", eventGroup->Id, (idOrder + "]").c_str());
#endif


	if (NOS_SYNC_NO_SYNC_EVENT_GROUP_ID != eventGroup->Id)
	{
		nosEngine.LogD("Resetting events of group %s", eventGroupStr);

		for (const auto& [eventId, event] : events)
		{
			auto res = event->Reset(event->UserData);
			if (res == NOS_RESULT_FAILED)
			{
				// Error when resetting, consensus cannot be achieved
				nosEngine.LogE("Failed to reset event %llu in group %s", eventId, eventGroupStr);
				return res;
			}
		}
	}

	nosEngine.LogD("Attempting to achieve consensus on event group %s", eventGroupStr);
	
	uint32_t syncedEventOccurrences = 0;
	uint64_t lastConsensusTimestamp = 0;
	uint64_t startTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

	if (events.empty())
	{
		nosEngine.LogE("No one is waiting on event group %s", eventGroupStr);
		return NOS_RESULT_NOT_FOUND; // No waiters to synchronize
	}

	std::unordered_map<Ref<Event>, uint64_t> eventTimestamps;
	eventTimestamps.reserve(events.size()); // Reserve space for all events
	// Collect initial timestamps for all events
	for (const auto& [eventId, event] : events)
	{
		auto waitRes = event->Wait(event->UserData);
		if (waitRes.Result == NOS_RESULT_FAILED)
		{
			// Error when waiting for an event, consensus cannot be achieved
			nosEngine.LogE("Failed to wait for event %llu in group %s", eventId, eventGroupStr);
			return waitRes.Result;
		}
		eventTimestamps[event] = waitRes.Timestamp;
	}

	uint64_t minTs = UINT64_MAX, maxTs = 0;
	while (true)
	{
		uint64_t currentTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
		auto currDiffFromStartNs = currentTimeNs - startTimeNs;
		auto eventIntervalNs = GetIntervalFromDeltaSecs(smallestDeltaSecs) * 1e9;
		auto frac = currDiffFromStartNs / eventIntervalNs;
		if (frac >= (1.0 + eventGroup->Timeout))
		{
			nosEngine.LogE("Timeout waiting for consensus on event group %s", eventGroupStr);
			return NOS_RESULT_TIMEOUT; // Timeout reached
		}
		
		// Find min and max timestamps
		minTs = UINT64_MAX;
		maxTs = 0;
		for (const auto& [event, ts] : eventTimestamps)
		{
			minTs = std::min(minTs, ts);
			maxTs = std::max(maxTs, ts);
		}

		// Check if consensus is achieved
		auto diffNs = (maxTs - minTs);
		if ((diffNs / eventIntervalNs <= eventGroup->Tolerance))
		{
			lastConsensusTimestamp = maxTs;
			auto time = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
				std::chrono::nanoseconds(lastConsensusTimestamp));
			std::string formatted = std::format("{:%H:%M:%S}", time);
			nosEngine.LogD("Consensus achieved on event group %s with timestamp %s with relative time span %.3f", eventGroupStr, formatted.c_str(), (diffNs / eventIntervalNs));
			*outTimestamp = event->LastWaitedTimestamp;
			*outCount = event->NumOccurrences;
			return NOS_RESULT_SUCCESS;
		}
		// Consensus is not achieved
		
		// Wait for the non-synchronized events again
		for (const auto& [eventId, event] : events)
		{
			// If the the event's timestamp is already within the tolarence compared to the maximum timestamp, skip it
			auto ts = eventTimestamps[event];
			auto difFrac = (maxTs - ts) / eventIntervalNs;
			if (difFrac <= eventGroup->Tolerance)
				continue;

			// Log the event that is behind
			{
				auto curTime =
					std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::nanoseconds(ts));
				auto maxTime =
					std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::nanoseconds(maxTs));
				nosEngine.LogD("Event group %s entry %llu is behind: Current %s, waiting for %s",
							   eventGroupStr,
							   event->Id,
							   std::format("{:%H:%M:%S}", curTime).c_str(),
							   std::format("{:%H:%M:%S}", maxTime).c_str());
			}

			auto waitRes = event->Wait(event->UserData);
			if (waitRes.Result == NOS_RESULT_FAILED)
			{
				// Error when waiting for an event, consensus cannot be achieved
				return waitRes.Result;
			}
			eventTimestamps[event] = waitRes.Timestamp;
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

extern "C"
{
NOSAPI_ATTR nosResult NOSAPI_CALL nosExportPlugin(nosPluginFunctions* funcs)
{
	funcs->OnRequestAPI = Export;
	funcs->Initialize = Initialize;
	funcs->OnPreUnloadPlugin = UnloadSubsystem;
	return NOS_RESULT_SUCCESS;
}
}
}
