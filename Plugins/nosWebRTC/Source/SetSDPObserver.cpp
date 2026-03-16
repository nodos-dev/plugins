// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include "SetSDPObserver.h"

nosSetSDPObserver::nosSetSDPObserver(int id) : peerConnectionID(id)
{
}

void nosSetSDPObserver::SetSuccessCallback(std::function<void(int)> callback)
{
	std::lock_guard lock(CallbackMutex);
	SuccessCallback = callback;
}

void nosSetSDPObserver::SetFailureCallback(std::function<void(webrtc::RTCError, int)> callback)
{
	std::lock_guard lock(CallbackMutex);
	FailureCallback = callback;
}

void nosSetSDPObserver::ClearCallbacks()
{
	std::lock_guard lock(CallbackMutex);
	SuccessCallback = {};
	FailureCallback = {};
}

void nosSetSDPObserver::OnSuccess()
{
	std::function<void(int)> callback;
	{
		std::lock_guard lock(CallbackMutex);
		callback = SuccessCallback;
	}
	if (callback) {
		callback(peerConnectionID);
	}
}

void nosSetSDPObserver::OnFailure(webrtc::RTCError error)
{
	std::function<void(webrtc::RTCError, int)> callback;
	{
		std::lock_guard lock(CallbackMutex);
		callback = FailureCallback;
	}
	if (callback) {
		callback(error, peerConnectionID);
	}
}

void nosSetSDPObserver::AddRef() const
{
	ref_count_.IncRef();
}

rtc::RefCountReleaseStatus nosSetSDPObserver::Release() const
{
	const auto status = ref_count_.DecRef();
	if (status == rtc::RefCountReleaseStatus::kDroppedLastRef) {
		delete this;
	}
	return status;
}
