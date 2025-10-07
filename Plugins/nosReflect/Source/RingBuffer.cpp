#include <Nodos/Plugin.hpp>
#include "Names.h"

#include <condition_variable>
#include <mutex>
#include <utility>

#include <nosTransfer/nosTransfer.h>

namespace nos::reflect
{
#include <vector>
#include <optional>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>

template <typename T>
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
		if (!FullCV.wait_for(lock, std::chrono::milliseconds(timeoutMs),
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
		lock.unlock();
		AvailableCV.notify_one();
	}

	T* BeginPop(uint32_t timeoutMs)
	{
		std::unique_lock lock(Mutex);
		if (!AvailableCV.wait_for(lock, std::chrono::milliseconds(timeoutMs),
			[this]() -> bool {
				return Size > 0 || ExitRequested.load();
			}))
			return nullptr; // timeout

		if (ExitRequested.load() && Size == 0)
			return nullptr;

		return &Buffer[Tail];
	}

	void EndPop()
	{
		std::unique_lock lock(Mutex);
		Tail = (Tail + 1) % Capacity;
		--Size;
		lock.unlock();
		FullCV.notify_one();
	}

	void Shutdown()
	{
		ExitRequested.store(true);
		AvailableCV.notify_all();
		FullCV.notify_all();
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
		lock.unlock();
		ExitRequested.store(false);
		AvailableCV.notify_all();
		FullCV.notify_all();
	}

private:
	size_t Capacity;
	std::vector<T> Buffer;
	size_t Head;
	size_t Tail;
	size_t Size;

	mutable std::mutex Mutex;
	std::condition_variable AvailableCV;
	std::condition_variable FullCV;
	std::atomic<bool> ExitRequested;
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
	nosResult CopyFrom(nosObjectHandle obj, uuid const& nodeId)
	{
		return nosTransfer->Copy(obj, Handle);
	}
	bool IsSlotCompatible(nosObjectHandle obj) const
	{
		return nosTransfer->CanCopy(obj, Handle) == NOS_TRUE;
	}
	ObjectRef GetObject() const
	{
		ObjectRef obj{};
		if (nosTransfer->GetObjectHandle(Handle, &obj.Handle) != NOS_RESULT_SUCCESS)
			return ObjectRef();
		return obj;
	}
};

struct RingBufferNode : NodeContext
{
	nosName TypeName = NSN_TypeNameGeneric;

	RingBuffer<std::unique_ptr<Slot>> Ring;
	uint32_t RingCapacity = 1;
	RingBufferNode() : Ring(1)
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

	void OnPathStart() override
	{
		Ring.Reset(RingCapacity);
		SendScheduleRequest(RingCapacity);
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
		if (auto srcSlot = Ring.BeginPop(100); srcSlot && *srcSlot)
		{
			auto& slot = *srcSlot;
			ObjectRef out;
			nosTransfer->GetObjectHandle(slot->Handle, &out.Handle);
			SetPinObject(NSN_Output, out.Handle);
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

	void OnPinObjectHandleChanged(nos::Name pinName, uuid const& pinId, nosObjectHandle handle) override
	{
		if (NOS_NAME("Capacity") == pinName)
		{
			auto newCapacity = *InterpretObject<uint32_t>(handle);
			if (newCapacity != RingCapacity)
			{
				RingCapacity = newCapacity;
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

		auto inputObject = ObjectRef(*params[NSN_Input].ObjectHandle);
		auto capacity = *InterpretObject<uint32_t>(*params[NOS_NAME("Capacity")].ObjectHandle);
		capacity = std::max(1u, capacity);

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

nosResult RegisterRingBuffer(nosNodeFunctions* funcs)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("RingBuffer"), RingBufferNode, funcs)
		return NOS_RESULT_SUCCESS;
}

}
