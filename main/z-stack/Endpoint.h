#pragma once

#include <cstdint>
#include <memory>

#include "ZStack.h"
#include "clusters.h"

class Endpoint {
public:
    Endpoint(uint64_t ieee,
            uint16_t nwaddr,
            uint16_t profileId,
            uint16_t deviceId,
            uint8_t endpoint,
             uint8_t numInClusters,
             std::unique_ptr<uint16_t[]> inClusters,
             uint8_t numOutClusters,
             std::unique_ptr<uint16_t[]> outClusters);

    const uint16_t* const getInClusters() const;
    const uint16_t* const getOutClusters() const;
    uint64_t getDeviceIeeeAddr() const;
    uint16_t getDeviceNetworkAddr() const;
    uint16_t getProfileId() const;
    uint16_t getDeviceId() const;
    uint8_t getEndpoint() const;
    uint8_t getNumInClusters() const;
    uint8_t getNumOutClusters() const;

    bool supportsInputCluster(ClusterId clusterId);

    ZclFrame read(ZStack* zstack, ClusterId clusterId, uint16_t* attributes, uint8_t attributesLen);
    void command(ZStack* zstack, ClusterId clusterId, ZclCommandId commandId, uint8_t* payload, uint8_t payloadLen);
//    ZclFrame commandWithResponse(ZStack* zstack, ClusterId clusterId, ZclCommandId commandId, uint8_t* payload, uint8_t payloadLen);
private:
    std::unique_ptr<uint16_t[]> inClusters;
    std::unique_ptr<uint16_t[]> outClusters;
    uint64_t deviceIeeeAddr;
    uint16_t deviceNetworkAddr;
    uint16_t profileId;
    uint16_t deviceId;
    uint8_t endpoint;
    uint8_t numInClusters;
    uint8_t numOutClusters;
};
