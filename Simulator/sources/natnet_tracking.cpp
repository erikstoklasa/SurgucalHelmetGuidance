
#include "natnet_tracking.hpp"

#include <utility>

NatNetTracking::NatNetTracking()
{
    client.SetFrameReceivedCallback(
        &NatNetTracking::onFrame, this
    );
}

NatNetTracking::~NatNetTracking()
{
    disconnect();
}

bool NatNetTracking::connect(
    const std::string& serverIP,
    const std::string& localIP)
{
    disconnect();

    if (serverIP.empty() || localIP.empty())
        return false;

    serverAddress = serverIP;
    localAddress = localIP;

    sNatNetClientConnectParams params;
    params.connectionType = ConnectionType_Unicast;
    params.serverAddress = serverAddress.c_str();
    params.localAddress = localAddress.c_str();

    if (client.Connect(params) != ErrorCode_OK) {
        client.Disconnect();
        return false;
    }

    sServerDescription description{};

    if (client.GetServerDescription(&description) != ErrorCode_OK ||
        !description.HostPresent) {
        client.Disconnect();
        return false;
    }

    connected = true;
    return true;
}

void NatNetTracking::disconnect()
{
    if (connected) {
        client.Disconnect();
        connected = false;
    }

    std::lock_guard<std::mutex> lock(mutex);
    hasNewFrame = false;
}

bool NatNetTracking::poll(TrackingFrame& frame)
{
    std::lock_guard<std::mutex> lock(mutex);

    if (!hasNewFrame)
        return false;

    frame = latestFrame;
    hasNewFrame = false;
    return true;
}

void NATNET_CALLCONV NatNetTracking::onFrame(
    sFrameOfMocapData* data,
    void* userData)
{
    if (!data || !userData)
        return;

    auto* self = static_cast<NatNetTracking*>(userData);

    TrackingFrame frame;
    frame.frameNumber = static_cast<uint32_t>(data->iFrame);
    frame.timestampSeconds = data->fTimestamp;

    frame.rigidBodies.reserve(data->nRigidBodies);

    for (int i = 0; i < data->nRigidBodies; ++i) {
        const auto& rb = data->RigidBodies[i];

        TrackedRigidBody tracked;
        tracked.id = static_cast<uint32_t>(rb.ID);

        tracked.position = {rb.x, rb.y, rb.z};
        tracked.rotation = {rb.qw, rb.qx, rb.qy, rb.qz};

        tracked.meanError = rb.MeanError;
        tracked.trackingValid = (rb.params & 0x01) != 0;

        frame.rigidBodies.push_back(tracked);
    }

    std::lock_guard<std::mutex> lock(self->mutex);
    self->latestFrame = std::move(frame);
    self->hasNewFrame = true;
}
