#pragma once

#include <condition_variable>
#include <mutex>
#include <vector>

#include <nosTransfer/nosTransfer.h>

#include "Names.h"

namespace nos::reflect
{
enum class ServeType
{
	WaitUntilFull, // Ring Buffer
	Immediate // Bounded Queue
};

template <typename T, ServeType Type = ServeType::WaitUntilFull>
class RingBuffer
{
public:
	explicit RingBuffer(size_t capacity)
		: Capacity(capacity),
		Buffer(capacity),
		Head(0),
		Tail(0),
		Size(0),
		ExitRequested(false)
	{
	}

	RingBuffer(const RingBuffer&) = delete;
	RingBuffer& operator=(const RingBuffer&) = delete;

	T* BeginPush(uint32_t timeoutMs)
	{
		std::unique_lock lock(Mutex);
		if (!ReadyForPushCV.wait_for(lock, std::chrono::milliseconds(timeoutMs),
			[this]() -> bool {
				return Size < Capacity || ExitRequested.load();
			}))
			return nullptr; // timeout

		if (ExitRequested.load())
			return nullptr;

		return &Buffer[Head];
	}

	void EndPush()
	{
		std::unique_lock lock(Mutex);
		Head = (Head + 1) % Capacity;
		++Size;
		if (!WaitUntilFull || Size == Capacity)
		{
			WaitUntilFull = false;
			ReadyForPopCV.notify_one();
		}
	}

	T* BeginPop(uint32_t timeoutMs)
	{
		std::unique_lock lock(Mutex);
		if (!ReadyForPopCV.wait_for(lock, std::chrono::milliseconds(timeoutMs),
			[this]() -> bool {
				if (ExitRequested)
					return true;
				if (WaitUntilFull)
					return Size == Capacity;
				return Size > 0;
			}))
			return nullptr; // timeout

		if (ExitRequested.load())
			return nullptr;

		return &Buffer[Tail];
	}

	void EndPop()
	{
		std::unique_lock lock(Mutex);
		Tail = (Tail + 1) % Capacity;
		if (Size > 0)
			--Size;
		lock.unlock();
		ReadyForPushCV.notify_one();
	}

	void Shutdown()
	{
		ExitRequested.store(true);
		ReadyForPopCV.notify_all();
		ReadyForPushCV.notify_all();
	}

	bool IsShuttingDown() const noexcept
	{
		return ExitRequested.load();
	}

	bool IsEmpty() const
	{
		std::unique_lock lock(Mutex);
		return Size == 0;
	}

	bool IsFull() const
	{
		std::unique_lock lock(Mutex);
		return Size == Capacity;
	}

	size_t GetSize() const
	{
		std::unique_lock lock(Mutex);
		return Size;
	}

	size_t GetCapacity() const noexcept
	{
		return Capacity;
	}

	void Reset(std::optional<size_t> newCapacity = std::nullopt)
	{
		std::unique_lock lock(Mutex);
		Head = 0;
		Tail = 0;
		Size = 0;
		Buffer.clear();
		if (newCapacity && *newCapacity != Capacity)
			Capacity = *newCapacity;
		Buffer.resize(Capacity);
		if constexpr (Type == ServeType::WaitUntilFull)
		{
			WaitUntilFull = true;
		}
		else if constexpr (Type == ServeType::Immediate)
		{
			WaitUntilFull = false;
		}
		lock.unlock();
		ExitRequested.store(false);
		ReadyForPopCV.notify_all();
		ReadyForPushCV.notify_all();
	}

private:
	size_t Capacity;
	std::vector<T> Buffer;
	size_t Head;
	size_t Tail;
	size_t Size;

	mutable std::mutex Mutex;
	std::condition_variable ReadyForPopCV;
	std::condition_variable ReadyForPushCV;
	std::atomic_bool ExitRequested;
	std::atomic_bool WaitUntilFull;
};

struct Slot
{
	nosTransferCopyDestination Handle;
	Slot(ObjectRef src)
	{
		nosTransfer->CreateCopyDestination(src, &Handle);
	}
	~Slot()
	{
		nosTransfer->ReleaseCopyDestination(Handle);
	}
	Slot(const Slot&) = delete;
	Slot& operator=(const Slot&) = delete;
	nosResult CopyFrom(nosObjectId obj, uuid const& nodeId)
	{
		return nosTransfer->Copy(obj, Handle);
	}
	bool IsSlotCompatible(nosObjectId obj) const
	{
		return nosTransfer->CanCopy(obj, Handle) == NOS_TRUE;
	}
	ObjectRef GetObject() const
	{
		ObjectRef obj{};
		if (nosTransfer->GetObjectReference(Handle, &obj.GetStorage()) != NOS_RESULT_SUCCESS)
			return ObjectRef();
		return obj;
	}
};


template <ServeType Type>
struct RingBufferNodeBase : NodeContext
{
	nosName TypeName = NSN_TypeNameGeneric;

	RingBuffer<std::unique_ptr<Slot>, Type> Ring;
	uint32_t Capacity = 1;
	RingBufferNodeBase() : Ring(1)
	{
	}

	void SendScheduleRequest(uint32_t count, bool reset = false) const
	{
		nosScheduleNodeParams schedule{
			.NodeId = NodeId,
			.AddScheduleCount = count,
			.Reset = reset
		};
		nosEngine.ScheduleNode(&schedule);
	}

	void SendRingStats(std::string_view state) const
	{
		auto nodeName = NodeName.AsString();
		nosEngine.WatchLog((nodeName + " Ring Size").c_str(), std::to_string(Ring.GetSize()).c_str());
		nosEngine.WatchLog((nodeName + " Ring Capacity").c_str(), std::to_string(Ring.GetCapacity()).c_str());
		nosEngine.WatchLog((nodeName + " State").c_str(), state.data());
	}

	void OnPathStart() override
	{
		Ring.Reset(Capacity);
		SendScheduleRequest(Capacity);
	}

	void OnPathStop() override
	{
		Ring.Shutdown();
	}

	nosResult OnCreate(nosFbNodePtr node) override
	{
		for (auto pin : *node->pins())
		{
			auto name = nos::Name(pin->name()->c_str());
			if (NSN_Output == name)
			{
				if (pin->type_name()->c_str() == NSN_TypeNameGeneric.AsString())
					continue;
				SetType(nos::Name(pin->type_name()->c_str()));
			}
		}
		return NOS_RESULT_SUCCESS;
	}

	nosResult CopyFrom(nosCopyFromInfo* cpy) override
	{
		SendRingStats("Pre Begin Pop");
		if (auto srcSlot = Ring.BeginPop(100); srcSlot && *srcSlot)
		{
			SendRingStats("Post Begin Pop");
			auto& slot = *srcSlot;
			ObjectRef out;
			nosTransfer->GetObjectReference(slot->Handle, &out.GetStorage());
			SetPinObject(NSN_Output, out);
			cpy->ShouldSetSourceFrameNumber = true;
			cpy->FrameNumber = 0; // TODO: Store frame number in ring buffer
			SendScheduleRequest(1);
			Ring.EndPop();
			return NOS_RESULT_SUCCESS;
		}
		if (Ring.IsShuttingDown())
			return NOS_RESULT_FAILED;
		return NOS_RESULT_PENDING;
	}

	void OnPinObjectChanged(nos::Name pinName, uuid const& pinId, nosObjectId handle) override
	{
		if (NOS_NAME("Capacity") == pinName)
		{
			auto newCapacity = *InterpretObject<uint32_t>(handle);
			if (newCapacity != Capacity)
			{
				Capacity = std::max(1u, newCapacity);
				SendPathRestart(NSN_Input);
			}
		}
	}

	void OnPinUpdated(nosPinUpdate const* update) override
	{
		if (TypeName != NSN_TypeNameGeneric)
			return;
		if (update->UpdatedField == NOS_PIN_FIELD_TYPE_NAME)
		{
			if (update->PinName != NSN_Input)
				return;
			SetType(update->TypeName);
		}
	}

	void SetType(nos::Name typeName)
	{
		TypeName = typeName;
	}

	nosResult ExecuteNode(NodeExecuteParams const& params) override
	{
		if (NSN_TypeNameGeneric == TypeName)
			return NOS_RESULT_FAILED;
		auto inputObject = ObjectRef(*params[NSN_Input].Object);
		if (!inputObject)
			return NOS_RESULT_FAILURE;
		auto capacity = *InterpretObject<uint32_t>(*params[NOS_NAME("Capacity")].Object);
		capacity = std::max(1u, capacity);

		SendRingStats("Pre Push");
		if (auto dstSlot = Ring.BeginPush(100))
		{
			if (!*dstSlot)
				*dstSlot = std::make_unique<Slot>(ObjectRef(inputObject));
			auto& slot = *dstSlot;
			if (!slot->IsSlotCompatible(inputObject))
			{
				SendPathRestart(NSN_Input);
				return NOS_RESULT_FAILURE;
			}
			auto res = slot->CopyFrom(inputObject, NodeId);
			Ring.EndPush();
			SendRingStats("Post Push");
			if (res != NOS_RESULT_SUCCESS)
				return res;
		}
		else if (Ring.IsShuttingDown())
		{
			return NOS_RESULT_FAILED;
		}
		else
		{
			// Timeout
			return NOS_RESULT_PENDING;
		}
		return NOS_RESULT_SUCCESS;
	}
};

}