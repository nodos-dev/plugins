#include <Nodos/Plugin.hpp>
#include "Names.h"

#include <condition_variable>
#include <mutex>
#include <utility>

#include <nosTransfer/nosTransfer.h>

namespace nos::reflect
{
struct RingBuffer
{
	struct Slot
	{
		ObjectRef Object;
		uint64_t FrameNumber = 0;
	};

	constexpr static int64_t NO_SLOT_INDEX = -1;

	std::mutex BufferMutex;
	std::deque<Slot> Buffer;
	std::condition_variable DataAvailable;
	std::condition_variable SpaceAvailable;
	uint32_t RingSize = 0;
	int64_t NextReadIdx = -1;
	int64_t NextWriteIdx = -1;
	std::atomic_bool Exit;

	Slot* BeginPop(uint32_t timeoutMs)
	{
		std::unique_lock lock(BufferMutex);
		auto success = DataAvailable.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this]
		{ 
			return NextReadIdx != NO_SLOT_INDEX || Exit;
		});
		if (!success || Exit)
			return nullptr;
		auto& slot = Buffer[NextReadIdx];
		NextReadIdx = (NextReadIdx + 1) % RingSize;
		return &slot;
	}
	
};

struct RingBufferNode : NodeContext
{
	nosName TypeName = NSN_TypeNameGeneric;
	

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
		/*std::unique_lock lock(BufferMutex);
		Exit = false;
		Buffer.clear();
		SendScheduleRequest(RingSize);*/
	}

	void OnPathStop() override
	{
		/*Exit = true;
		DataAvailable.notify_all();
		SpaceAvailable.notify_all();*/
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
		//std::unique_lock lock(BufferMutex);
		//
		//// Wait until data is available in the buffer
		//auto success = DataAvailable.wait_for(lock, std::chrono::milliseconds(100), [this]
		//{ 
		//	return !Buffer.empty() || Exit;
		//});
		//if (!success)
		//	return NOS_RESULT_PENDING;
		//if (Exit)
		//	return NOS_RESULT_FAILED;
		//
		//// Pop from front of buffer
		//Slot slot = Buffer.front();
		//Buffer.pop_front();
		//
		//// Notify that space is now available
		//SpaceAvailable.notify_one();
		//
		//// Set the output pin object handle to the popped object
		//SetPinObject(NSN_Output, slot.Object.Handle);
		//
		//cpy->ShouldSetSourceFrameNumber = true;
		//cpy->FrameNumber = slot.FrameNumber;

		//SendScheduleRequest(1);
		return NOS_RESULT_SUCCESS;
	}
	
	void OnPinObjectHandleChanged(nos::Name pinName, uuid const& pinId, nosObjectHandle handle) override
	{
		//if (NSN_Size == pinName)
		//{
		//	std::unique_lock lock(BufferMutex);
		//	auto newSize = *InterpretObject<uint32_t>(handle);
		//	if (newSize != RingSize)
		//		SendPathRestart(NSN_Input);
		//	RingSize = newSize;
		//	
		//	Buffer.clear();
		//	
		//	// Notify waiting threads about potential space availability
		//	SpaceAvailable.notify_all();
		//}
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
		//if (NSN_TypeNameGeneric == TypeName)
		//	return NOS_RESULT_FAILED;

		//auto inputObject = ObjectRef(*params[NSN_Input].ObjectHandle);
		//auto size = *InterpretObject<uint32_t>(*params[NSN_Size].ObjectHandle);
		//size = std::max(1u, size);

		//std::unique_lock lock(BufferMutex);
		//
		//// Wait until space is available in the buffer (ring is not full)
		//auto success = SpaceAvailable.wait_for(lock, std::chrono::milliseconds(100), [this, size]
		//{ 
		//	return Buffer.size() < size || Exit;
		//});
		//if (!success)
		//	return NOS_RESULT_PENDING;
		//if (Exit)
		//	return NOS_RESULT_FAILED;

		//if (!Buffer.empty())
		//{
		//	auto& front = Buffer.front();
		//	if (nosTransfer->CopyAPI->CanCopy(inputObject, front.Object.Handle) == NOS_FALSE)
		//	{
		//		// If we cannot copy, we need to restart the path to avoid data corruption
		//		Buffer.clear();
		//		SendPathRestart(NSN_Input);
		//		return NOS_RESULT_PENDING;
		//	}
		//}

		//while (Buffer.size() < size)
		//{
		//	// Clone input object and copy into it
		//	auto cloned = inputObject.Clone();
		//	auto res = nosTransfer->CopyAPI->Copy(inputObject, cloned.Handle);
		//	if (res != NOS_RESULT_SUCCESS)
		//		return res;
		//	Buffer.push_back({std::move(cloned), params.FrameNumber});
		//	DataAvailable.notify_one();
		//}

		//auto slot = std::move(Buffer.front());

		//auto cloned = inputObject.Clone();

		//// Create new slot with input object and current frame number
		//Slot newSlot;
		//newSlot.Object = std::move(cloned);
		//newSlot.FrameNumber = params.FrameNumber;

		//// Notify that data is now available
		//DataAvailable.notify_one();
		return NOS_RESULT_SUCCESS;
	}
};

nosResult RegisterRingBuffer(nosNodeFunctions* funcs)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("RingBuffer"), RingBufferNode, funcs)
	return NOS_RESULT_SUCCESS;
}

}