#include "Endpoint.h"
#include "ZclFrame.h"

Endpoint::Endpoint(uint64_t ieee, uint16_t nwaddr, uint16_t profileId, uint16_t deviceId, uint8_t endpoint,
                   uint8_t numInClusters, std::unique_ptr<uint16_t[]> inClusters, uint8_t numOutClusters,
                   std::unique_ptr<uint16_t[]> outClusters):
                   inClusters(std::move(inClusters)),
                   outClusters(std::move(outClusters)),
                   deviceIeeeAddr(ieee),
                   deviceNetworkAddr(nwaddr),
                   profileId(profileId),
                   deviceId(deviceId),
                   endpoint(endpoint),
                   numInClusters(numInClusters),
                   numOutClusters(numOutClusters)
                   {

}

const uint16_t* const Endpoint::getInClusters() const {
    return inClusters.get();
}

const uint16_t* const Endpoint::getOutClusters() const {
    return outClusters.get();
}

uint64_t Endpoint::getDeviceIeeeAddr() const {
    return deviceIeeeAddr;
}

uint16_t Endpoint::getDeviceNetworkAddr() const {
    return deviceNetworkAddr;
}

uint16_t Endpoint::getProfileId() const {
    return profileId;
}

uint16_t Endpoint::getDeviceId() const {
    return deviceId;
}

uint8_t Endpoint::getEndpoint() const {
    return endpoint;
}

uint8_t Endpoint::getNumInClusters() const {
    return numInClusters;
}

uint8_t Endpoint::getNumOutClusters() const {
    return numOutClusters;
}

bool Endpoint::supportsInputCluster(ClusterId clusterId) {
    for(int i = 0; i < numInClusters; i++) {
        if(ClusterId(inClusters.get()[i]) == clusterId)
            return true;
    }
    return false;
}

ZclFrame Endpoint::read(ZStack* zstack, ClusterId clusterId, uint16_t* attributes, uint8_t attributesLen) {

    ZclFrame frame(
            ZclFrameType::GLOBAL,
            Direction::CLIENT_TO_SERVER,
            true,
            0,
            zstack->getNextTransactionSequence(),
            clusterId,
            ZclCommandId::GLOBAL_READ, (uint8_t*)attributes, attributesLen*2);

    return zstack->sendZclFrameNetworkAddressWithResponse(deviceNetworkAddr, endpoint, frame, 10000);

}

void Endpoint::command(ZStack* zstack, ClusterId clusterId, ZclCommandId commandId, uint8_t *payload, uint8_t payloadLen) {
    ZclFrame frame(ZclFrameType::SPECIFIC,
            Direction::CLIENT_TO_SERVER,
            true,
            0,
            zstack->getNextTransactionSequence(),
            clusterId,
            commandId,
            payload,
            payloadLen);

    zstack->sendZclFrameNetworkAddress(deviceNetworkAddr, endpoint, frame, 10000);
}