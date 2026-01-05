#include <Nodos/Plugin.hpp>

#include <mutex>

#include "nosSync/Sync_generated.h"

#include "Promise.hpp"

namespace nos::sync
{

struct PromiseObject
{
	std::mutex Mutex;
	std::condition_variable Condition;
	bool IsFulfilled{ false };
	std::string Tag;

	PromiseObject(const char* tag)
		: Tag(tag)
	{
	}

	~PromiseObject()
	{
		// Optional safety measure: wake up any waiting threads
		Fulfill();
	}

	void Reset()
	{
		{
			std::unique_lock lock(Mutex);
			IsFulfilled = false;
		}
		Condition.notify_all();
	}

	nosResult Wait(uint64_t timeoutNs)
	{
		std::unique_lock lock(Mutex);
		bool success = Condition.wait_for(
			lock,
			std::chrono::nanoseconds(timeoutNs),
			[this] { return IsFulfilled; }
		);

		if (success)
		{
			// Automatically reset after successful wait
			IsFulfilled = false;
			return NOS_RESULT_SUCCESS;
		}

		return NOS_RESULT_TIMEOUT;
	}

	void Fulfill()
	{
		{
			std::unique_lock lock(Mutex);
			IsFulfilled = true;
		}
		Condition.notify_all();
	}

	EngineBuffer Serialize() const
	{
		return EngineBuffer::CopyFrom(nos::Buffer::From(TPromise{.tag = Tag}));
	}
};

nosResult NOSAPI_CALL ConstructPromiseObject(nosBuffer buffer, nosForeignHandle* outForeignHandle, nosBuffer* outSerializedData)
{
	if (!outForeignHandle)
		return NOS_RESULT_INVALID_ARGUMENT;
	auto promise = InterpretObjectData<Promise>(buffer);
	auto obj = new PromiseObject(promise->tag() ? promise->tag()->c_str() : "Untagged");
	*outForeignHandle = static_cast<nosForeignHandle>(obj);
	*outSerializedData = obj->Serialize().Release();
	return NOS_RESULT_SUCCESS;
}

void NOSAPI_CALL ReleasePromiseObject(nosForeignHandle foreignHandle)
{
	auto* promise = static_cast<PromiseObject*>(foreignHandle);
	delete promise;
}

nosResult NOSAPI_CALL CreatePromise(const char* tag, nosObjectReference* outPromise)
{
	// TODO: Pass tag
	auto promise = new PromiseObject(tag);
	return nosEngine.ObjectAPI->CreateObjectForForeignHandle(NOS_NAME("nos.sync.Promise"), promise, promise->Serialize(), outPromise);
}

nosResult NOSAPI_CALL WaitPromise(nosObjectId promise, uint64_t timeoutNs)
{
	nosForeignHandle foreignHandle;
	auto res = nosEngine.ObjectAPI->GetForeignHandle(promise, &foreignHandle);
	if (res != NOS_RESULT_SUCCESS)
		return res;
	auto* promiseObj = static_cast<PromiseObject*>(foreignHandle);
	return promiseObj->Wait(timeoutNs);
}

nosResult NOSAPI_CALL FulfillPromise(nosObjectId promise)
{
	nosForeignHandle foreignHandle;
	auto res = nosEngine.ObjectAPI->GetForeignHandle(promise, &foreignHandle);
	if (res != NOS_RESULT_SUCCESS)
		return res;
	auto* promiseObj = static_cast<PromiseObject*>(foreignHandle);
	promiseObj->Fulfill();
	return NOS_RESULT_SUCCESS;
}

nosResult NOSAPI_CALL ResetPromise(nosObjectId promise)
{
	nosForeignHandle foreignHandle;
	auto res = nosEngine.ObjectAPI->GetForeignHandle(promise, &foreignHandle);
	if (res != NOS_RESULT_SUCCESS)
		return res;
	auto* promiseObj = static_cast<PromiseObject*>(foreignHandle);
	promiseObj->Reset();
	return NOS_RESULT_SUCCESS;
}
}
