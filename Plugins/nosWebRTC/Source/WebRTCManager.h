/*
 * Copyright MediaZ Teknoloji A.S. All Rights Reserved.
 */

#pragma once
#include <mutex>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "api/media_stream_interface.h"
#include "api/peer_connection_interface.h"
#include "CustomVideoSource.h"
#include "WebRTCClient.h"
#include "CreateSDPObserver.h"
#include "SetSDPObserver.h"
#include "PeerConnectionObserver.h"
#include "CustomVideoSink.h"

typedef rtc::scoped_refptr<webrtc::PeerConnectionInterface> PeerConnectionPtr;

struct nosPeerConnectionState {
    int PeerID = -1;
    std::shared_ptr<nosPeerConnectionObserver> Observer;
    rtc::scoped_refptr<nosCreateSDPObserver> CreateSDPObserver;
    rtc::scoped_refptr<nosSetSDPObserver> SetSDPObserver;
    PeerConnectionPtr PeerConnection;
};

class nosWebRTCManager {
public:

    nosWebRTCManager(nosWebRTCClient* p_nosWebRTCClient);
    ~nosWebRTCManager();

    bool MainLoop(int cms=0);

    void SendOffer(int id);
    void UpdateBitrates(int bitrateKBPS);
    void AddVideoSource(rtc::scoped_refptr<nosCustomVideoSource> source);
    void AddVideoSink(rtc::scoped_refptr<nosCustomVideoSink> sink);

    void SetPeerConnectedCallback(std::function<void()> callback);
    void SetPeerDisconnectedCallback(std::function<void()> callback);
    void SetServerConnectionSuccesfulCallback(std::function<void()> callback);
    void SetServerConnectionFailedCallback(std::function<void()> callback);
    void Dispose();

    void AddRef() const;
    rtc::RefCountReleaseStatus Release() const;
protected:

    bool AddPeerConnection(int& connectionID);
    void RemovePeerConnection(int connectionID);

    void OnImageEncoded();

    #pragma region nosWebRTCClient Region

    void RegisterToWebRTCClientCallbacks();
    void OnServerConnectionSuccesful();
    void OnServerConnectionError();
    void OnServerConnectionClosed();
    void OnRawMessageReceived(void* data, size_t length);
    void OnSDPOfferReceived(std::string&& offer);
    void OnSDPAnswerReceived(std::string&& answer);
    void OnICECandidateReceived(std::string&& iceCandidate);
    void OnPeerDisconnectedReceived(std::string&& peerDisconnected);

    #pragma endregion

    #pragma region nosPeerConnectionObserver Region
   
    void RegisterToPeerConnectionObserverCallbacks(nosPeerConnectionObserver* observer);
    void OnSignalingChange( webrtc::PeerConnectionInterface::SignalingState new_state, int id);
    void OnAddTrack( rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
        const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>& streams, int id);
    void OnRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver, int id);
    void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> channel, int id);
    void OnRenegotiationNeeded(int id);
    void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state, int id);
    void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state, int id);
    void OnIceCandidate(const webrtc::IceCandidateInterface* candidate, int id);

    #pragma endregion
    
    #pragma region nosCreateSDPObserver Region
    void RegisterToCreateSDPObserverCallbacks(nosCreateSDPObserver* observer);
    // Will be called when SDP creation is succesful.
    void OnSDPCreateSuccess(webrtc::SessionDescriptionInterface* desc, int id);
    void OnSDPCreateFailure(webrtc::RTCError error, int id);
    #pragma endregion

    #pragma region nosSetSDPObserver Region
    void RegisterToSetSDPObserverCallbacks(nosSetSDPObserver* observer);
    //Will be called when SDP set to PeerConnection succesfully
    void OnSDPSetSuccess(int id);
    void OnSDPSetFailure(webrtc::RTCError error, int id);
    #pragma endregion
    // Send a message to the signaling server.

    bool TryGetPeerConnectionState(int connectionID, nosPeerConnectionState& state) const;
    bool TryGetConnectionIDFromPeerID(int& connectionID, int peerID) const;
    void AttachPeerIDToConnection(int connectionID, int peerID);
    bool RemovePeerConnectionByPeerID(int peerID);
    void ReleasePeerConnectionState(int connectionID, nosPeerConnectionState* removedState = nullptr);
    std::vector<PeerConnectionPtr> GetPeerConnectionsSnapshot() const;
    
    std::unique_ptr<rtc::Thread> SignalingThread;
    std::unique_ptr<rtc::Thread> WorkerThread;
    //We will register ourselves so that we will be notified whether SDP creation succeed or failed
    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> p_PeerConnectionFactory;
    mutable std::mutex PeerConnectionsMutex;
    std::unordered_map<int, nosPeerConnectionState> p_PeerConnections;
    std::unordered_map<int, int> p_PeerIDToConnectionID;
    int NextPeerConnectionID = 0;

    std::vector<rtc::scoped_refptr<nosCustomVideoSource>> p_VideoSources;
    std::vector<rtc::scoped_refptr<nosCustomVideoSink>> p_VideoSinks;
    std::vector<rtc::scoped_refptr<webrtc::VideoTrackInterface>> p_VideoTracks;

    nosWebRTCClient* p_nosWebRTCClient;
    
    int targetKbps = 5000;
    bool IsDisposed = false;

    std::function<void()> ImageEncodeCompletedCallback;
    std::function<void()> PeerConnectedCallback;
    std::function<void()> PeerDisconnectedCallback;
    std::function<void()> ServerConnectionSuccesfulCallback;
    std::function<void()> ServerConnectionFailedCallback;
    mutable webrtc::webrtc_impl::RefCounter ref_count_{ 0 };

    friend nosPeerConnectionObserver;
    friend nosCreateSDPObserver;
    friend nosSetSDPObserver;

};
