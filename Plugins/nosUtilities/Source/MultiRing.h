/*
 * Copyright MediaZ Teknoloji A.S. All Rights Reserved.
 */

#pragma once

#include "Ring.h"

namespace nos
{

// Ring that holds N independent channels under a single mutex / CV pair.
// Each channel still owns its own slot pools and Resources, but every push,
// pop, resize and reset goes through the shared synchronization, so an
// N-channel batch push is a single lock acquisition, not N.
struct MultiRing
{
	struct Channel
	{
		std::shared_ptr<ResourceInterface> ResInterface;
		std::vector<rc<ResourceInterface::ResourceBase>> Resources;
		std::deque<ResourceInterface::ResourceBase*> WritePool;
		std::deque<ResourceInterface::ResourceBase*> ReadPool;
		void* UserData = nullptr;
	};

	std::map<char, std::unique_ptr<Channel>> Channels;
	std::mutex Mutex;
	std::condition_variable WriteCV;
	std::condition_variable ReadCV;
	std::atomic_bool Exit = true;
	uint32_t Size = 0;

	~MultiRing() { Stop(); }

	void Stop()
	{
		{
			std::unique_lock lock(Mutex);
			Exit = true;
		}
		WriteCV.notify_all();
		ReadCV.notify_all();
	}

	void Start()
	{
		std::unique_lock lock(Mutex);
		Exit = false;
	}

	void AllocateChannelResourcesUnlocked(Channel& ch)
	{
		ch.WritePool.clear();
		ch.ReadPool.clear();
		ch.Resources.clear();
		for (uint32_t i = 0; i < Size; ++i)
		{
			auto res = ch.ResInterface->CreateResource();
			if (!res)
			{
				nosEngine.LogE("Failed to create resource for multi ring buffer.");
				ch.Resources.clear();
				ch.WritePool.clear();
				ch.ReadPool.clear();
				Exit = true;
				return;
			}
			ch.Resources.push_back(res);
			ch.WritePool.push_back(res.get());
		}
	}

	Channel& AddChannel(char key, std::shared_ptr<ResourceInterface> resInterface, void* userData = nullptr)
	{
		std::unique_lock lock(Mutex);
		auto& ch = Channels[key];
		if (!ch)
			ch = std::make_unique<Channel>();
		ch->ResInterface = std::move(resInterface);
		ch->UserData = userData;
		if (Size == 0)
			Size = 1;
		AllocateChannelResourcesUnlocked(*ch);
		return *ch;
	}

	void RemoveChannel(char key)
	{
		std::unique_lock lock(Mutex);
		Channels.erase(key);
	}

	void RecreateChannelResources(Channel& ch)
	{
		std::unique_lock lock(Mutex);
		AllocateChannelResourcesUnlocked(ch);
	}

	void ResizeAll(uint32_t newSize)
	{
		std::unique_lock lock(Mutex);
		Size = newSize;
		for (auto& [_, ch] : Channels)
			AllocateChannelResourcesUnlocked(*ch);
	}

	bool AreAllChannelsValid()
	{
		std::unique_lock lock(Mutex);
		if (Channels.empty())
			return false;
		for (auto& [_, ch] : Channels)
			if (ch->Resources.empty())
				return false;
		return true;
	}

	// Move slots between pools for every channel. fill=false: read→write.
	void ResetAll(bool fill)
	{
		std::unique_lock lock(Mutex);
		for (auto& [_, ch] : Channels)
		{
			auto& from = fill ? ch->WritePool : ch->ReadPool;
			auto& to = fill ? ch->ReadPool : ch->WritePool;
			while (!from.empty())
			{
				auto* slot = from.front();
				from.pop_front();
				ch->ResInterface->Reset(slot);
				to.push_back(slot);
			}
		}
	}

	// If this channel is full and its read pool is non-empty, hand one slot
	// back to the write pool so the producer can start pushing again.
	void MoveOneReadToWriteIfFull(Channel& ch)
	{
		std::unique_lock lock(Mutex);
		if (ch.ReadPool.size() != ch.Resources.size() || ch.ReadPool.empty())
			return;
		auto* slot = ch.ReadPool.front();
		ch.ReadPool.pop_front();
		ch.WritePool.push_back(slot);
	}

	bool IsFull(Channel const& ch)
	{
		std::unique_lock lock(Mutex);
		return ch.ReadPool.size() == ch.Resources.size();
	}

	bool IsEmpty(Channel const& ch)
	{
		std::unique_lock lock(Mutex);
		return ch.ReadPool.empty();
	}

	size_t WritePoolSize(Channel const& ch)
	{
		std::unique_lock lock(Mutex);
		return ch.WritePool.size();
	}

	size_t ReadPoolSize(Channel const& ch)
	{
		std::unique_lock lock(Mutex);
		return ch.ReadPool.size();
	}

	using SlotPair = std::pair<Channel*, ResourceInterface::ResourceBase*>;

	// Atomically pop one slot from each requested channel's WritePool.
	// Waits until every requested channel has at least one slot, or
	// timeout/exit. The caller-supplied list typically excludes channels
	// that don't have valid input data this frame.
	bool BeginPushSubset(uint64_t timeoutMs,
						 std::vector<Channel*> const& wanted,
						 std::vector<SlotPair>& outSlots)
	{
		std::unique_lock lock(Mutex);
		auto pred = [&] {
			if (Exit)
				return true;
			if (wanted.empty())
				return false;
			for (auto* ch : wanted)
				if (ch->WritePool.empty())
					return false;
			return true;
		};
		if (!WriteCV.wait_for(lock, std::chrono::milliseconds(timeoutMs), pred))
			return false;
		if (Exit)
			return false;
		outSlots.clear();
		outSlots.reserve(wanted.size());
		for (auto* ch : wanted)
		{
			auto* slot = ch->WritePool.front();
			ch->WritePool.pop_front();
			outSlots.emplace_back(ch, slot);
		}
		return true;
	}

	void EndPushAll(std::vector<SlotPair> const& slots)
	{
		{
			std::unique_lock lock(Mutex);
			for (auto& [ch, slot] : slots)
				ch->ReadPool.push_back(slot);
		}
		ReadCV.notify_all();
	}

	void CancelPushAll(std::vector<SlotPair> const& slots)
	{
		{
			std::unique_lock lock(Mutex);
			for (auto& [ch, slot] : slots)
			{
				slot->FrameNumber = 0;
				ch->WritePool.push_front(slot);
			}
		}
		WriteCV.notify_all();
	}

	ResourceInterface::ResourceBase* BeginPop(Channel& ch, uint64_t timeoutMs)
	{
		std::unique_lock lock(Mutex);
		if (!ReadCV.wait_for(lock, std::chrono::milliseconds(timeoutMs),
							 [&] { return !ch.ReadPool.empty() || Exit; }))
			return nullptr;
		if (Exit)
			return nullptr;
		auto* slot = ch.ReadPool.front();
		ch.ReadPool.pop_front();
		return slot;
	}

	void EndPop(Channel& ch, ResourceInterface::ResourceBase* slot)
	{
		{
			std::unique_lock lock(Mutex);
			slot->FrameNumber = 0;
			ch.WritePool.push_back(slot);
		}
		WriteCV.notify_all();
	}
};

} // namespace nos
