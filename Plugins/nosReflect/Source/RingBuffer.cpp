#include <Nodos/Plugin.hpp>
#include "Names.h"

#include <condition_variable>
#include <mutex>
#include <utility>

namespace nos::reflect
{

struct RingBufferNode : NodeContext
{
	nosName TypeName = NSN_TypeNameGeneric;
	
	struct Slot
	{
		ObjectRef Object;
		uint64_t FrameNumber = 0;
	};

	std::mutex BufferMutex;
	std::condition_variable DataAvailable;
	std::condition_variable SpaceAvailable;
	std::deque<Slot> Buffer;
	uint32_t RingSize = 0;
	std::atomic_bool Exit;

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
		std::unique_lock lock(BufferMutex);
		Exit = false;
		Buffer.clear();
		SendScheduleRequest(RingSize);
	}

	void OnPathStop() override
	{
		Exit = true;
		DataAvailable.notify_all();
		SpaceAvailable.notify_all();
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
		std::unique_lock<std::mutex> lock(BufferMutex);
		
		// Wait until data is available in the buffer
		auto success = DataAvailable.wait_for(lock, std::chrono::milliseconds(100), [this]
		{ 
			return !Buffer.empty() || Exit;
		});
		if (!success)
			return NOS_RESULT_PENDING;
		if (Exit)
			return NOS_RESULT_FAILED;
		
		// Pop from front of buffer
		Slot slot = Buffer.front();
		Buffer.pop_front();
		
		// Notify that space is now available
		SpaceAvailable.notify_one();
		
		// Set the output pin object handle to the popped object
		SetPinObject(NSN_Output, slot.Object.Handle);
		
		cpy->ShouldSetSourceFrameNumber = true;
		cpy->FrameNumber = slot.FrameNumber;

		SendScheduleRequest(1);
		return NOS_RESULT_SUCCESS;
	}
	
	void OnPinObjectHandleChanged(nos::Name pinName, uuid const& pinId, nosObjectHandle handle) override
	{
		if (NSN_Size == pinName)
		{
			std::lock_guard<std::mutex> lock(BufferMutex);
			RingSize = *InterpretObject<uint32_t>(handle);
			
			// Trim buffer if new size is smaller
			while (Buffer.size() > RingSize && RingSize > 0)
			{
				Buffer.pop_front();
			}
			
			// Notify waiting threads about potential space availability
			SpaceAvailable.notify_all();
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

		auto& inputObject = *params[NSN_Input].ObjectHandle;
		auto size = *InterpretObject<uint32_t>(*params[NSN_Size].ObjectHandle);
		size = std::max(1u, size);

		std::unique_lock<std::mutex> lock(BufferMutex);
		
		// Wait until space is available in the buffer (ring is not full)
		auto success = SpaceAvailable.wait_for(lock, std::chrono::milliseconds(100), [this, size]
		{ 
			return Buffer.size() < size || Exit;
		});
		if (!success)
			return NOS_RESULT_PENDING;
		if (Exit)
			return NOS_RESULT_FAILED;

		// Create new slot with input object and current frame number
		Slot newSlot;
		newSlot.Object = ObjectRef(inputObject);
		newSlot.FrameNumber = params.FrameNumber;
		
		// Push to back of buffer
		Buffer.push_back(newSlot);
		
		// If buffer exceeds ring size, remove from front to maintain ring behavior
		while (Buffer.size() > size)
		{
			Buffer.pop_front();
		}
		
		// Notify that data is now available
		DataAvailable.notify_one();
		return NOS_RESULT_SUCCESS;
	}
};

nosResult RegisterRingBuffer(nosNodeFunctions* funcs)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("RingBuffer"), RingBufferNode, funcs)
	return NOS_RESULT_SUCCESS;
}

}