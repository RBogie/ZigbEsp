//
// Created by Rob Bogie on 31/01/2020.
//

#include <iostream>
#include "Device.h"

Device::Device(uint64_t ieeeAddr, uint16_t networkAddress):
ieeeAddr(ieeeAddr),
networkAddress(networkAddress),
interviewed(false),
interviewInProgress(false) {

}

uint64_t Device::getIeeeAddr() const {
    return ieeeAddr;
}

uint16_t Device::getNetworkAddress() const {
    return networkAddress;
}

void Device::setNetworkAddress(uint16_t networkAddress) {
    Device::networkAddress = networkAddress;
}

const std::time_t &Device::getLastSeen() const {
    return lastSeen;
}

void Device::updateLastSeen() {
    lastSeen = std::time(nullptr);
}

bool Device::isInterviewed() {
    return interviewed;
}

bool Device::isInterviewInProgress() {
    return interviewInProgress;
}

void Device::interview(ZStack &zstack) {
    interviewInProgress = true;
    zstack.addTask([this, &zstack]{
        bool nodeDescriptorReceived = false;
        for(int i = 0; i < 6; i++) {
            //Try 5 times, because some devices don't respond in 1 try
            try {
                auto descriptor = zstack.getNodeDescriptor(networkAddress);
                this->type = descriptor.type;
                this->manufacturerCode = descriptor.manufacturerCode;
                nodeDescriptorReceived = true;
                break;
            } catch(std::runtime_error& err) {

            }
            interviewInProgress = false;
        }

        if(!nodeDescriptorReceived) {
            interviewInProgress = false;
            return;
        }

        ZdoActiveEpRspReq activeEps;
        bool epsRetrieved = false;
        for(uint8_t i = 0; i < 2; i++) {
            //Some devices don't respond on first try, do 2 attempts
            try {
                std::cout << "Trying to get endpoints" << std::endl;
                activeEps = zstack.getActiveEndpoints(networkAddress);
                epsRetrieved = true;
                break;
            } catch(std::runtime_error& e) {
                std::cerr << "Error: " << e.what() << std::endl;

            }
        }

        if(!epsRetrieved) {
            std::cout << "Could not retrieve active endpoints of this device" << std::endl;
        }

        for (uint8_t i = 0; i < activeEps.activeepcount; i++) {
            const uint8_t endpoint = activeEps.activeeplist[i];
            std::cout << "Trying to get simple descriptor for endpoint " << (int)endpoint << std::endl;
            auto endpointDescriptor = zstack.getSimpleDescriptor(ieeeAddr, networkAddress, endpoint);
            std::cout << "inClusters: ";
            auto inClusters = endpointDescriptor.getInClusters();
            for(int i = 0; i < endpointDescriptor.getNumInClusters(); i++) {
                std::cout << (int)inClusters[i] << ", ";
            }
            std::cout << std::endl;

            std::cout << "outClusters: ";
            auto outClusters = endpointDescriptor.getOutClusters();
            for(int i = 0; i < endpointDescriptor.getNumOutClusters(); i++) {
                std::cout << (int)outClusters[i] << ", ";
            }
            std::cout << std::endl;
            endpoints.push_back(std::move(endpointDescriptor));
        }

        if(endpoints.empty()) {
            std::cout << "Interview of device halted. No active endpoints found" << std::endl;
            interviewInProgress = false;
            return;
        }

        for(auto& endpoint : endpoints) {
            if(!endpoint.supportsInputCluster(ClusterId::genBasic))
                continue;

            //Since the payloads might become quite large, we split our requests into 3 partial data requests.
            retrieveModelIdManufacturerPowerSource(zstack, endpoint);
            retrieveZclAppStackVersions(zstack, endpoint);
            retrieveHwVersionDateCodeSwBuildId(zstack, endpoint);
            break;
        }

        //TODO: do IAS enrollment

        interviewed = true;
        interviewInProgress = false;
    });
}

DeviceLogicalType Device::getType() const {
    return type;
}

uint16_t Device::getManufacturerCode() const {
    return manufacturerCode;
}


void Device::retrieveModelIdManufacturerPowerSource(ZStack &zstack, Endpoint &endpoint) {
    uint16_t attributes[] = {
            (uint16_t)GenBasicAttrId::modelId,
            (uint16_t)GenBasicAttrId::manufacturerName,
            (uint16_t)GenBasicAttrId::powerSource
    };

    auto attrData = endpoint.read(&zstack, ClusterId::genBasic, attributes, 3);
    uint8_t index = 0;
    for(int i = 0; i < 3; i++) {
        GenBasicAttrId attr;
        try {
            auto d = parseReadRspEntry(attrData.getPayload(), attrData.getPayloadLength(), &index, (uint16_t *) &attr);
            switch(attr) {
                case GenBasicAttrId::modelId:
                    std::cout << "Retrieved modelId: " << d.getStr() << std::endl;
                    modelId = std::string(d.getStr());
                    break;
                case GenBasicAttrId::manufacturerName:
                    std::cout << "Retrieved manufacturerName: " << d.getStr() << std::endl;
                    manufacturer = std::string(d.getStr());
                    break;
                case GenBasicAttrId::powerSource:
                    std::cout << "Retrieved powerSource: " << (int)d.getUint8Val() << std::endl;
                    powerSource = PowerSource(d.getUint8Val());
                    break;
                default:
                    break; //Should never happen
            }
        } catch(std::runtime_error& e) {
            std::cerr << "Error retrieving attr:" << e.what() << std::endl;
        }

    }
}

void Device::retrieveZclAppStackVersions(ZStack &zstack, Endpoint &endpoint) {
    uint16_t attributes[] = {
            (uint16_t)GenBasicAttrId::zclVersion,
            (uint16_t)GenBasicAttrId::appVersion,
            (uint16_t)GenBasicAttrId::stackVersion
    };

    auto attrData = endpoint.read(&zstack, ClusterId::genBasic, attributes, 3);
    uint8_t index = 0;
    for(int i = 0; i < 3; i++) {
        GenBasicAttrId attr;
        try {
            auto d = parseReadRspEntry(attrData.getPayload(), attrData.getPayloadLength(), &index, (uint16_t *) &attr);
            switch(attr) {
                case GenBasicAttrId::zclVersion:
                    std::cout << "Retrieved zclVersion: " << (int)d.getUint8Val() << std::endl;
                    zclVersion = d.getUint8Val();
                    break;
                case GenBasicAttrId::appVersion:
                    std::cout << "Retrieved appVersion: " << (int)d.getUint8Val() << std::endl;
                    appVersion = d.getUint8Val();
                    break;
                case GenBasicAttrId::stackVersion:
                    std::cout << "Retrieved stackVersion: " << (int)d.getUint8Val() << std::endl;
                    stackVersion = d.getUint8Val();
                    break;
                default:
                    break; //Should never happen
            }
        } catch(std::runtime_error& e) {
            std::cerr << "Error retrieving attr:" << e.what() << std::endl;
        }

    }
}

void Device::retrieveHwVersionDateCodeSwBuildId(ZStack &zstack, Endpoint &endpoint) {
    uint16_t attributes[] = {
            (uint16_t)GenBasicAttrId::hwVersion,
            (uint16_t)GenBasicAttrId::dateCode,
            (uint16_t)GenBasicAttrId::swBuildId
    };

    auto attrData = endpoint.read(&zstack, ClusterId::genBasic, attributes, 3);
    uint8_t index = 0;
    for(int i = 0; i < 3; i++) {
        GenBasicAttrId attr;
        try {
            auto d = parseReadRspEntry(attrData.getPayload(), attrData.getPayloadLength(), &index, (uint16_t *) &attr);
            switch(attr) {
                case GenBasicAttrId::hwVersion:
                    std::cout << "Retrieved hwVersion: " << (int)d.getUint8Val() << std::endl;
                    hwVersion = d.getUint8Val();
                    break;
                case GenBasicAttrId::dateCode:
                    std::cout << "Retrieved dateCode: " << d.getStr() << std::endl;
                    swDateCode = std::string(d.getStr());
                    break;
                case GenBasicAttrId::swBuildId:
                    std::cout << "Retrieved swBuildId: " << d.getStr() << std::endl;
                    swBuildId = std::string(d.getStr());
                    break;
                default:
                    break; //Should never happen
            }
        } catch(std::runtime_error& e) {
            std::cerr << "Error retrieving attr:" << e.what() << std::endl;
        }

    }
}

Endpoint* Device::getEndpoint(ClusterId clusterId) {
    for(auto& endpoint : endpoints) {
        if(endpoint.supportsInputCluster(clusterId))
            return &endpoint;
    }
    return nullptr;
}