// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include "CreateSDPObserver.h"

nosCreateSDPObserver::nosCreateSDPObserver(int id) : peerConnectionID(id)
{
}

void nosCreateSDPObserver::SetSuccessCallback(std::function<void(webrtc::SessionDescriptionInterface*, int)> callback)
{
	std::lock_guard lock(CallbackMutex);
	SuccessCallback = callback;
}

void nosCreateSDPObserver::SetFailureCallback(std::function<void(webrtc::RTCError, int)> callback)
{
	std::lock_guard lock(CallbackMutex);
	FailureCallback = callback;
}

void nosCreateSDPObserver::ClearCallbacks()
{
	std::lock_guard lock(CallbackMutex);
	SuccessCallback = {};
	FailureCallback = {};
}

void nosCreateSDPObserver::OnSuccess(webrtc::SessionDescriptionInterface* desc)
{
	std::function<void(webrtc::SessionDescriptionInterface*, int)> callback;
	{
		std::lock_guard lock(CallbackMutex);
		callback = SuccessCallback;
	}
	if (callback) {
		callback(desc, peerConnectionID);
	}
}

void nosCreateSDPObserver::OnFailure(webrtc::RTCError error)
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

void nosCreateSDPObserver::AddRef() const
{
	ref_count_.IncRef();
}

rtc::RefCountReleaseStatus nosCreateSDPObserver::Release() const
{
	const auto status = ref_count_.DecRef();
	if (status == rtc::RefCountReleaseStatus::kDroppedLastRef) {
		delete this;
	}
	return status;
}
