// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include "PeerConnectionObserver.h"
nosPeerConnectionObserver::nosPeerConnectionObserver(int _peerConnectionID) :peerConnectionID(_peerConnectionID)
{
}

void nosPeerConnectionObserver::ClearCallbacks()
{
	std::lock_guard lock(CallbackMutex);
	SignalingChangeCallback = {};
	AddTrackCallback = {};
	RemoveTrackCallback = {};
	DataChannelCallback = {};
	RenegotiationNeededCallback = {};
	IceConnectionChangeCallback = {};
	IceGatheringChangeCallback = {};
	IceCandidateCallback = {};
}

void nosPeerConnectionObserver::OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state)
{
	std::function<void(webrtc::PeerConnectionInterface::SignalingState, int)> callback;
	{
		std::lock_guard lock(CallbackMutex);
		callback = SignalingChangeCallback;
	}
	if (callback) {
		callback(new_state, peerConnectionID);
	}
}

void nosPeerConnectionObserver::OnAddTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver, const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>& streams)
{
	std::function<void(rtc::scoped_refptr<webrtc::RtpReceiverInterface>, const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>&, int)> callback;
	{
		std::lock_guard lock(CallbackMutex);
		callback = AddTrackCallback;
	}
	if (callback) {
		callback(receiver, streams, peerConnectionID);
	}
}

void nosPeerConnectionObserver::OnRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver)
{
	std::function<void(rtc::scoped_refptr<webrtc::RtpReceiverInterface>, int)> callback;
	{
		std::lock_guard lock(CallbackMutex);
		callback = RemoveTrackCallback;
	}
	if (callback) {
		callback(receiver, peerConnectionID);
	}
}

void nosPeerConnectionObserver::OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel)
{
	std::function<void(rtc::scoped_refptr<webrtc::DataChannelInterface>, int)> callback;
	{
		std::lock_guard lock(CallbackMutex);
		callback = DataChannelCallback;
	}
	if (callback) {
		callback(data_channel, peerConnectionID);
	}
}

void nosPeerConnectionObserver::OnRenegotiationNeeded()
{
	std::function<void(int)> callback;
	{
		std::lock_guard lock(CallbackMutex);
		callback = RenegotiationNeededCallback;
	}
	if (callback) {
		callback(peerConnectionID);
	}
}

void nosPeerConnectionObserver::OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state)
{
	std::function<void(webrtc::PeerConnectionInterface::IceConnectionState, int)> callback;
	{
		std::lock_guard lock(CallbackMutex);
		callback = IceConnectionChangeCallback;
	}
	if (callback) {
		callback(new_state, peerConnectionID);
	}
}

void nosPeerConnectionObserver::OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state)
{
	std::function<void(webrtc::PeerConnectionInterface::IceGatheringState, int)> callback;
	{
		std::lock_guard lock(CallbackMutex);
		callback = IceGatheringChangeCallback;
	}
	if (callback) {
		callback(new_state, peerConnectionID);
	}
}

void nosPeerConnectionObserver::OnIceCandidate(const webrtc::IceCandidateInterface* candidate)
{
	std::function<void(const webrtc::IceCandidateInterface*, int)> callback;
	{
		std::lock_guard lock(CallbackMutex);
		callback = IceCandidateCallback;
	}
	if (callback) {
		callback(candidate, peerConnectionID);
	}
}

void nosPeerConnectionObserver::SetSignalingChangeCallback(std::function<void(webrtc::PeerConnectionInterface::SignalingState, int)> callback)
{
	std::lock_guard lock(CallbackMutex);
	SignalingChangeCallback = callback;
}

void nosPeerConnectionObserver::SetAddTrackCallback(std::function<void(rtc::scoped_refptr<webrtc::RtpReceiverInterface>, const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>&, int)> callback)
{
	std::lock_guard lock(CallbackMutex);
	AddTrackCallback = callback;
}

void nosPeerConnectionObserver::SetRemoveTrackCallback(std::function<void(rtc::scoped_refptr<webrtc::RtpReceiverInterface>, int)> callback)
{
	std::lock_guard lock(CallbackMutex);
	RemoveTrackCallback = callback;
}

void nosPeerConnectionObserver::SetDataChannelCallback(std::function<void(rtc::scoped_refptr<webrtc::DataChannelInterface>, int)> callback)
{
	std::lock_guard lock(CallbackMutex);
	DataChannelCallback = callback;
}

void nosPeerConnectionObserver::SetRenegotiationCallback(std::function<void(int)> callback)
{
	std::lock_guard lock(CallbackMutex);
	RenegotiationNeededCallback = callback;
}

void nosPeerConnectionObserver::SetICEConnectionChangeCallback(std::function<void(webrtc::PeerConnectionInterface::IceConnectionState, int)> callback)
{
	std::lock_guard lock(CallbackMutex);
	IceConnectionChangeCallback = callback;
}

void nosPeerConnectionObserver::SetICEGatheringChangeCallback(std::function<void(webrtc::PeerConnectionInterface::IceGatheringState, int)> callback)
{
	std::lock_guard lock(CallbackMutex);
	IceGatheringChangeCallback = callback;
}

void nosPeerConnectionObserver::SetICECandidateCallback(std::function<void(const webrtc::IceCandidateInterface*, int)> callback)
{
	std::lock_guard lock(CallbackMutex);
	IceCandidateCallback = callback;
}
