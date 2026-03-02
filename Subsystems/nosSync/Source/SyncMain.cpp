// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#include <nosSync/nosSync.h>

#include <Nodos/SubsystemAPI.h>

#include <shared_mutex>
#include <format>
#include <numeric>
#include <random>

#define RANDOMIZE_EVENT_ORDER 0 // For diagnosis, set to 1 to randomize the order of events when waiting for consensus

NOS_INIT_WITH_MIN_REQUIRED_MINOR(0);

NOS_BEGIN_IMPORT_DEPS()
NOS_END_IMPORT_DEPS()

namespace nos::sync
{
std::unordered_map<uint32_t, nosSyncSubsystem*> GExportedSubsystemVersions;

uint64_t NowNs()
{
	return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// Helper to compare two rational time steps (a/b < c/d) using cross-multiplication
static int CompareDeltaSeconds(nosVec2u d1, nosVec2u d2)
{
	if (d1.y == 0 || d2.y == 0)
		return 0;
	uint64_t left = static_cast<uint64_t>(d1.x) * d2.y;
	uint64_t right = static_cast<uint64_t>(d2.x) * d1.y;
	return (left < right) ? -1 : (left > right ? 1 : 0);
}

static nosVec2u ReduceDeltaSeconds(nosVec2u deltaSecs)
{
	if (deltaSecs.y == 0)
		return deltaSecs;
	auto gcd = std::gcd(deltaSecs.x, deltaSecs.y);
	return { deltaSecs.x / gcd, deltaSecs.y / gcd };
}

nosVec2u GetLcmDeltaSeconds(nosVec2u d1, nosVec2u d2)
{
	if (d1.y == 0 || d2.y == 0)
		return {0, 0};
	auto d1Reduced = ReduceDeltaSeconds(d1);
	auto d2Reduced = ReduceDeltaSeconds(d2);
	// Cast to uint64_t to prevent overflow during LCM calculation
	uint32_t lcmx = static_cast<uint32_t>(std::lcm<uint64_t>(d1Reduced.x, d2Reduced.x));
	uint32_t gcdy = std::gcd(d1Reduced.y, d2Reduced.y);
	return {lcmx, gcdy};
}

nosVec2u GetGcdDeltaSeconds(nosVec2u d1, nosVec2u d2)
{
	if (d1.y == 0 || d2.y == 0)
		return {0, 0};
	auto d1Reduced = ReduceDeltaSeconds(d1);
	auto d2Reduced = ReduceDeltaSeconds(d2);
	uint32_t gcdx = std::gcd(d1Reduced.x, d2Reduced.x);
	// Cast to uint64_t to prevent overflow during LCM calculation
	uint32_t lcmy = static_cast<uint32_t>(std::lcm<uint64_t>(d1Reduced.y, d2Reduced.y));
	return {gcdx, lcmy};
}

// Determines if two time steps can align, meaning one is an integer multiple of the other
bool CanTimeStepsAlign(nosVec2u d1, nosVec2u d2)
{
	if (d1.y == 0 || d2.y == 0 || d1.x == 0 || d2.x == 0)
		return false;

	auto d1Reduced = ReduceDeltaSeconds(d1);
	auto d2Reduced = ReduceDeltaSeconds(d2);

	uint64_t ad = static_cast<uint64_t>(d1Reduced.x) * d2Reduced.y;
	uint64_t bc = static_cast<uint64_t>(d1Reduced.y) * d2Reduced.x;

	return (ad % bc == 0) || (bc % ad == 0);
}

nosVec2u GetSmallerDeltaSeconds(nosVec2u d1, nosVec2u d2) { return (CompareDeltaSeconds(d1, d2) <= 0) ? d1 : d2; }

nosVec2u GetLargerDeltaSeconds(nosVec2u d1, nosVec2u d2) { return (CompareDeltaSeconds(d1, d2) >= 0) ? d1 : d2; }

double GetIntervalFromDeltaSecs(nosVec2u deltaSecs)
{
	if (deltaSecs.y == 0)
		return 0.0;										   // Avoid division by zero
	return static_cast<double>(deltaSecs.x) / deltaSecs.y; // Convert delta-seconds to seconds
}

struct WaitResult
{
	nosResult Result;
	uint64_t Timestamp;
};

struct Event
{
	uint64_t Id;
	uint64_t PathGroupId = 0;
	uint32_t EventGroupId = 0;
	nosResetEventPfn PfnReset = nullptr; // Optional callback for resetting the event
	nosEventWaitPfn PfnWait;
	nosNotifyEventGroupHealthPfn PfnNotifyEventGroupHealth = nullptr; // Optional callback for notifying about event group health
	void* UserData;
	nosVec2u DeltaSeconds = { 0, 0 }; // Time units in delta-seconds (x, y) for this event
	uint64_t LastWaitedTimestamp = 0; // Last timestamp when this event was waited on
	uint64_t OccurenceCountAtSync = 0; // Number of times this event was fired when consensus achieved
	bool HasRequestedConsensus = false; // Whether the caller has requested consensus

	std::deque<uint64_t> LastOccurrences; // Timestamps of recent waits for this event, used for health checking
	
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
				OccurenceCountAtSync = outRes.EventCount;
				res.Timestamp = ts;
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
	void AddEvent(Event&& event)
	{
		Events.emplace(event.Id, std::move(event));
		RecomputeLcmGcd();
	}
	void RemoveEvent(uint64_t eventId)
	{
		auto it = Events.find(eventId);
		if (it != Events.end())
		{
			Events.erase(it);
			RecomputeLcmGcd();
		}
	}
	void RecomputeLcmGcd()
	{
		LcmDeltaSeconds = { 0, 0 };
		GcdDeltaSeconds = { 0, 0 };
		for (auto& [eid, e] : Events)
		{
			if (LcmDeltaSeconds.x == 0 && LcmDeltaSeconds.y == 0)
			{
				LcmDeltaSeconds = e.DeltaSeconds;
				GcdDeltaSeconds = e.DeltaSeconds;
			}
			else
			{
				LcmDeltaSeconds = GetLcmDeltaSeconds(LcmDeltaSeconds, e.DeltaSeconds);
				GcdDeltaSeconds = GetGcdDeltaSeconds(GcdDeltaSeconds, e.DeltaSeconds);
			}
		}
	}
	nosVec2u LcmDeltaSeconds = { 0, 0 }; // Least common multiple of delta-seconds of all events in this group, used for health checking
	nosVec2u GcdDeltaSeconds = { 0, 0 }; // Greatest common divisor of delta-seconds of all events in this group, used for health checking
};

std::string GetEventGroupString(Ref<Event> event, Ref<EventGroup> eventGroup, nosVec2u smallestDeltaSecs)
{
	char eventGroupStr[256];
	std::snprintf(eventGroupStr, sizeof(eventGroupStr), "[%llu:%lu:(%lu/%lu)]", event->PathGroupId, eventGroup->Id, smallestDeltaSecs.x, smallestDeltaSecs.y);
	return std::string(eventGroupStr);
}

struct EventSync
{
	std::shared_mutex Mutex;
	uint64_t NextEventId = 1;
	std::unordered_map<uint32_t, EventGroup> Groups;
	
	std::unordered_map<uint64_t, std::vector<uint64_t>> AlignedTimestamps; // Event Id x List of timestamps when events occurred (aligned)

	std::variant<std::pair<Ref<Event>, Ref<EventGroup>>, nosResult> GetEvent(uint64_t eventId)
	{
		Event* event = nullptr;
		EventGroup* eventGroup = nullptr;
		for (auto git = Groups.begin(); git != Groups.end(); ++git)
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
		if (!event || !eventGroup)
			return NOS_RESULT_NOT_FOUND;
		return std::make_pair(Ref(*event), Ref(*eventGroup));
	}

	std::unordered_map<uint64_t, Ref<Event>> GetSyncedEvents(Ref<Event> event, nosVec2u* outSmallestDeltaSecs = nullptr)
	{
		std::unordered_map<uint64_t, Ref<Event>> events;
		nosVec2u smallestDeltaSecs = event->DeltaSeconds;
		auto it = Groups.find(event->EventGroupId);
		if (it == Groups.end())
			return events; // Event group not found, return empty
		auto& group = it->second;
		for (auto& [eid, e] : group.Events)
		{
			if (group.Id == NOS_SYNC_NO_SYNC_EVENT_GROUP_ID)
			{
				if (e.Id != event->Id)
					continue;
				else
					events.emplace(eid, e);
			}
			else
			{
				if (e.PathGroupId == event->PathGroupId
					&& CanTimeStepsAlign(e.DeltaSeconds, event->DeltaSeconds))
				{
					// Only consider events with the same delta-seconds and in same connected component
					events.emplace(eid, e);
					smallestDeltaSecs = GetSmallerDeltaSeconds(smallestDeltaSecs, e.DeltaSeconds);
				}
			}
		}
		if (outSmallestDeltaSecs)
			*outSmallestDeltaSecs = smallestDeltaSecs;
		return events;
	}
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
	eventGroup.Events[nextId] = {
		.Id = nextId,
		.PathGroupId = *pathGroupId,
		.EventGroupId = eventGroup.Id,
		.PfnReset = params->ResetFn,
		.PfnWait = params->WaitFn,
		.PfnNotifyEventGroupHealth = params->NotifyHealthFn,
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
	auto eventRes = GEventSync.GetEvent(eventId);
	if (std::holds_alternative<nosResult>(eventRes))
		return std::get<nosResult>(eventRes); // Event not found
	auto& [event, eventGroup] = std::get<std::pair<Ref<Event>, Ref<EventGroup>>>(eventRes);

	// Gather the events that should be considered for consensus.
	// They should be in the same group and have the same delta-seconds.
	nosVec2u smallestDeltaSecs;
	auto events = GEventSync.GetSyncedEvents(event, &smallestDeltaSecs);

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
			*outCount = event->OccurenceCountAtSync;
		return NOS_RESULT_SUCCESS; // Already waited for consensus
	}

	auto eventGroupStr = GetEventGroupString(event, eventGroup, smallestDeltaSecs);

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
		nosEngine.LogD("Resetting events of group %s", eventGroupStr.c_str());

		for (const auto& [eventId, event] : events)
		{
			auto res = event->Reset(event->UserData);
			if (res == NOS_RESULT_FAILED)
			{
				// Error when resetting, consensus cannot be achieved
				nosEngine.LogE("Failed to reset event %llu in group %s", eventId, eventGroupStr.c_str());
				return res;
			}
		}
	}

	nosEngine.LogD("Attempting to achieve consensus on event group %s", eventGroupStr.c_str());
	
	uint64_t lastConsensusTimestamp = 0;
	uint64_t startTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

	if (events.empty())
	{
		nosEngine.LogE("No one is waiting on event group %s", eventGroupStr.c_str());
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
			nosEngine.LogE("Failed to wait for event %llu in group %s", eventId, eventGroupStr.c_str());
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
			nosEngine.LogE("Timeout waiting for consensus on event group %s", eventGroupStr.c_str());
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
			nosEngine.LogD("Consensus achieved on event group %s with timestamp %s with relative time span %.3f", eventGroupStr.c_str(), formatted.c_str(), (diffNs / eventIntervalNs));
			*outTimestamp = event->LastWaitedTimestamp;
			*outCount = event->OccurenceCountAtSync;
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
							   eventGroupStr.c_str(),
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
}

nosResult NOSAPI_CALL NotifyEventOccured(uint64_t eventId)
{
	auto now = NowNs();
	std::unique_lock lock(GEventSync.Mutex);
	auto eventRes = GEventSync.GetEvent(eventId);
	if (std::holds_alternative<nosResult>(eventRes))
		return std::get<nosResult>(eventRes); // Event not found
	auto& [event, eventGroup] = std::get<std::pair<Ref<Event>, Ref<EventGroup>>>(eventRes);
	event->LastOccurrences.push_back(now);
	auto syncedEvents = GEventSync.GetSyncedEvents(event);
	// For synced events, how many we need to wait for that specific event to consider the cycle complete.
	std::unordered_map<uint64_t, uint32_t> multipliers;
	nosVec2u smallestDeltaSecs {UINT32_MAX, 1};
	for (const auto& [eid, ev] : syncedEvents)
		smallestDeltaSecs = GetSmallerDeltaSeconds(smallestDeltaSecs, ev->DeltaSeconds);
	for (const auto& [eid, ev] : syncedEvents)
	{
		// How many times this event should occur within the time window defined by the smallest delta-seconds to be considered in consensus for this cycle.
		multipliers[eid] = (ev->DeltaSeconds.y == 0 || smallestDeltaSecs.y == 0)
			? 1 
			: (smallestDeltaSecs.x * ev->DeltaSeconds.y) / (smallestDeltaSecs.y * ev->DeltaSeconds.x);
	}
	uint32_t fulfilledCount = 0;
	for (const auto& [eid, ev] : syncedEvents)
	{
		if (ev->LastOccurrences.size() >= multipliers[eid])
			fulfilledCount++;
	}
	if (fulfilledCount == syncedEvents.size()) // Everyone is ready.
	{
		uint64_t maxEventTimeDifferenceNs = 0;
		std::unordered_map<uint64_t, uint64_t> timestamps;
		for (const auto& [eid, ev] : syncedEvents)
		{
			uint32_t rem = multipliers[eid];
			while (rem > 0 && !ev->LastOccurrences.empty())
			{
				auto ts = ev->LastOccurrences.front();
				ev->LastOccurrences.pop_front();
				if (rem == 1)
					timestamps[eid] = ts;
				rem--;
			}
		}
		// Check the maximum time difference between the events that are considered in consensus for this cycle, and notify the health status if supported.
		for (const auto& [eid, ts] : timestamps)
		{
			auto diff = (ts > event->LastWaitedTimestamp) ? (ts - event->LastWaitedTimestamp) : (event->LastWaitedTimestamp - ts);
			maxEventTimeDifferenceNs = std::max(maxEventTimeDifferenceNs, diff);
			// Moving average of timestamps
			GEventSync.AlignedTimestamps[eid].push_back(ts);
			if (GEventSync.AlignedTimestamps[eid].size() > 10)
				GEventSync.AlignedTimestamps[eid].erase(GEventSync.AlignedTimestamps[eid].begin());
		}
		nosBool driftDetected = NOS_FALSE;
		for (const auto& [eid, alignedTimestamps] : GEventSync.AlignedTimestamps)
		{
			if (alignedTimestamps.size() < 2)
				continue;
			// To check drift, calculate whether any of the alignedTimestamps are consistently moving apart from other events' alignedTimestamps over time.
			
		}
		for (const auto& [eid, ev] : syncedEvents)
		{
			if (ev->PfnNotifyEventGroupHealth)
			{
				nosEventGroupHealth healthInfo{
					.DriftDetected = NOS_FALSE, // TODO.
					.MaxEventTimeDifferenceNs = maxEventTimeDifferenceNs
				};
				ev->PfnNotifyEventGroupHealth(ev->UserData, &healthInfo);
			}
		}
	}
	return NOS_RESULT_SUCCESS;
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
	subsystem->NotifyEventOccured = NotifyEventOccured;
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
NOSAPI_ATTR nosResult NOSAPI_CALL nosExportSubsystem(nosSubsystemFunctions* funcs)
{
	funcs->OnRequest = Export;
	funcs->Initialize = Initialize;
	funcs->OnPreUnloadSubsystem = UnloadSubsystem;
	return NOS_RESULT_SUCCESS;
}
}
}
