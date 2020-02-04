#pragma once

#include <cstdint>
#include <ctime>
#include <vector>

#include "ZStack.h"
#include "Endpoint.h"

class Device {
public:
    Device(uint64_t ieeeAddr, uint16_t networkAddress);

    uint64_t getIeeeAddr() const;
    uint16_t getNetworkAddress() const;
    void setNetworkAddress(uint16_t networkAddress);
    const std::time_t &getLastSeen() const;
    void updateLastSeen();
    bool isInterviewed();
    bool isInterviewInProgress();
    void interview(ZStack& zstack);
    DeviceLogicalType getType() const;
    uint16_t getManufacturerCode() const;

    Endpoint* getEndpoint(ClusterId clusterId);
private:
    std::vector<Endpoint> endpoints;
    std::string modelId;
    std::string manufacturer;
    std::string swDateCode;
    std::string swBuildId;

    const uint64_t ieeeAddr;
    uint16_t networkAddress;

    std::time_t lastSeen;
    bool interviewed;
    bool interviewInProgress;

    DeviceLogicalType type;
    uint16_t manufacturerCode;
    PowerSource powerSource;
    uint8_t zclVersion;
    uint8_t appVersion;
    uint8_t stackVersion;
    uint8_t hwVersion;

    void retrieveModelIdManufacturerPowerSource(ZStack& zstack, Endpoint& endpoint);
    void retrieveZclAppStackVersions(ZStack& zstack, Endpoint& endpoint);
    void retrieveHwVersionDateCodeSwBuildId(ZStack& zstack, Endpoint& endpoint);
};