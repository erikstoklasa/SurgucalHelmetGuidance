
#pragma once

#include "../core/tracking_frame.hpp"

#include <NatNetTypes.h>
#include <NatNetClient.h>

#include <mutex>
#include <string>

class NatNetTracking {
public:
    NatNetTracking();
    ~NatNetTracking();

    bool connect(const std::string& serverAddress,
                 const std::string& localAddress);

    void disconnect();
    bool poll(TrackingFrame& frame);

private:
    static void NATNET_CALLCONV onFrame(
        sFrameOfMocapData* data,
        void* userData
    );

    NatNetClient client;
    std::mutex mutex;
    TrackingFrame latestFrame;
    bool hasNewFrame = false;
    bool connected = false;

    std::string serverAddress;
    std::string localAddress;
};
