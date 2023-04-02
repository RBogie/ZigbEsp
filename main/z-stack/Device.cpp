//
// Created by Rob Bogie on 31/01/2020.
//

#include <iostream>
#include <cstring>
#include "Device.h"

static const char* BASE58_MAP = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

static uint8_t ieeeToStr(uint64_t ieee, char* buff, uint8_t buffLen) {
    if(buffLen < 12) {
        //buffer possible to small, error out
        return 0;
    }
    uint8_t numBytes = 0;
    uint8_t byteIndex = 0;
    uint8_t totalNumBytes = 0;
    
    for(int i = 0; i < 8; i++) {
        uint16_t b = (ieee >> (8*(7-i))) & 0xff;
        if(numBytes == totalNumBytes && !b) {
            buff[numBytes++] = '1';
            totalNumBytes++;
            continue;
        }
        uint8_t buffIndex = numBytes;
        while(buffIndex < totalNumBytes || b) {
            uint16_t n;
            if(buffIndex < totalNumBytes) {
                n = buff[buffIndex] * 256 + b;
            } else {
                n = b;
            }
            b = n / 58;
            buff[buffIndex] = n % 58;
            buffIndex++;
            if(buffIndex > totalNumBytes)
                totalNumBytes = buffIndex;
        }
    }
    
    int currentIndex = numBytes;
    int endIndex = totalNumBytes;
    while(currentIndex < endIndex) {
        char lastChar = BASE58_MAP[(uint8_t)buff[currentIndex]];
        buff[currentIndex] = BASE58_MAP[(uint8_t)buff[endIndex - 1]];
        buff[endIndex - 1] = lastChar;
        currentIndex++;
        endIndex--;
    }
    buff[totalNumBytes] = 0;
    return totalNumBytes;
}

Device::Device(uint64_t ieeeAddr, uint16_t networkAddress):
ieeeAddr(ieeeAddr),
networkAddress(networkAddress),
interviewed(false),
interviewInProgress(false) {

}

uint64_t Device::getIeeeAddr() const {
    return ieeeAddr;
}

std::string Device::getIeeeAddrStr() const {
    char name[12];
    if(!ieeeToStr(ieeeAddr, name, 12)) {
        throw std::runtime_error("Could not convert ieeeAddr");
    }
    return std::string(name);
}

std::string& Device::getModelId() {
    return modelId;
}
std::string& Device::getManufacturer() {
    return manufacturer;
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

void Device::interview(ZStack &zstack, std::function<void(Device*)> onInterviewDone) {
    interviewInProgress = true;
    zstack.addTask([this, &zstack, onInterviewDone]{
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

        if(onInterviewDone) {
            onInterviewDone(this);
        }
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

std::vector<const Endpoint*> Device::getEndpoints() const {
    std::vector<const Endpoint*> endpoints;
    endpoints.reserve(this->endpoints.size());
    for(auto& endpoint : this->endpoints) {
        endpoints.push_back(&endpoint);
    }
    return endpoints;
}

Endpoint* Device::getEndpoint(ClusterId clusterId) {
    for(auto& endpoint : endpoints) {
        if(endpoint.supportsInputCluster(clusterId))
            return &endpoint;
    }
    return nullptr;
}

void Device::saveToStorage(nvs_handle storageHandle) {
    this->printInfo();
    // Save order should be:
    // Fixed size:
    // - ieeeAddr (8 bytes)
    // - networkAddress (2 bytes)
    // - lastSeen (4 bytes)
    // - interviewed (1 byte)
    // - type (1 byte)
    // - manufacturerCode(2 bytes)
    // - powerSource (1 byte)
    // - zclVersion (1 byte)
    // - appVersion (1 byte)
    // - stackVersion (1 byte)
    // - hwVersion (1 byte)
    // Dynamic strings:
    // - modelId
    // - manufacturer
    // - swDateCode
    // - swBuildId
    //
    // - Number of endpoints
    // For each endpoint:
    // -Fixed size:
    // -- deviceIeeeAddr (8 bytes)
    // -- deviceNetworkAddr (2 bytes)
    // -- profileId (2 bytes)
    // -- deviceId (2 bytes)
    // -- endpoint (1 byte)
    // -- numInClusters (1 byte)
    // -- numOutClusters (1 byte)
    // -Dynamic:
    // -- inClusters
    // -- outClusters

    //Calculate size needed
    uint16_t sizeNeeded = 23 + 4 + modelId.length() + manufacturer.length() + swDateCode.length() + swBuildId.length() + 1;

    for(auto& endpoint : endpoints) {
        sizeNeeded += 7 + (endpoint.getNumInClusters() * 2) + (endpoint.getNumOutClusters() * 2);
    }

    uint8_t *data = new uint8_t[sizeNeeded];
    size_t index = 0;
    *((uint64_t*)&(data[index])) = ieeeAddr;
    index += 8;
    *((uint16_t*)&(data[index])) = networkAddress;
    index += 2;
    *((uint32_t*)&(data[index])) = lastSeen;
    index += 4;
    *((uint8_t*)&(data[index++])) = interviewed;
    *((uint8_t*)&(data[index++])) = (uint8_t)type;
    *((uint16_t*)&(data[index])) = manufacturerCode;
    index += 2;
    *((uint8_t*)&(data[index++])) = (uint8_t)powerSource;
    *((uint8_t*)&(data[index++])) = (uint8_t)zclVersion;
    *((uint8_t*)&(data[index++])) = (uint8_t)appVersion;
    *((uint8_t*)&(data[index++])) = (uint8_t)stackVersion;
    *((uint8_t*)&(data[index++])) = (uint8_t)hwVersion;
    *((uint8_t*)&(data[index++])) = (uint8_t)modelId.length();

    memcpy(&(data[index]), modelId.c_str(), modelId.length());
    index += modelId.length();
    *((uint8_t*)&(data[index++])) = (uint8_t)manufacturer.length();
    memcpy(&(data[index]), manufacturer.c_str(), manufacturer.length());
    index += manufacturer.length();
    *((uint8_t*)&(data[index++])) = (uint8_t)swDateCode.length();
    memcpy(&(data[index]), swDateCode.c_str(), swDateCode.length());
    index += swDateCode.length();
    *((uint8_t*)&(data[index++])) = (uint8_t)swBuildId.length();
    memcpy(&(data[index]), swBuildId.c_str(), swBuildId.length());
    index += swBuildId.length();

    *((uint8_t*)&(data[index++])) = (uint8_t)endpoints.size();
    //Write endpoints
    for(auto& endpoint : endpoints) {
        *((uint16_t*)&(data[index])) = endpoint.getProfileId();
        index += 2;
        *((uint16_t*)&(data[index])) = endpoint.getDeviceId();
        index += 2;
        *((uint8_t*)&(data[index++])) = endpoint.getEndpoint();
        *((uint8_t*)&(data[index++])) = endpoint.getNumInClusters();
        *((uint8_t*)&(data[index++])) = endpoint.getNumOutClusters();
        memcpy(&(data[index]), endpoint.getInClusters(), endpoint.getNumInClusters()*2);
        index += endpoint.getNumInClusters()*2;
        memcpy(&(data[index]), endpoint.getOutClusters(), endpoint.getNumOutClusters()*2);
        index += endpoint.getNumOutClusters()*2;
    }

    char name[12];
    if(!ieeeToStr(ieeeAddr, name, 12)) {
        throw std::runtime_error("Could not convert ieeeAddr");
    }

    std::cout << "Saving device " << name << std::endl;

    if(nvs_set_blob(storageHandle, name, data, index) != ESP_OK)
        throw std::runtime_error("Error while saving device");

    delete[] data;
}

std::shared_ptr<Device> Device::restoreFromStorage(uint8_t* buff, size_t buffLen) {
    size_t index = 0;
    uint64_t ieeeAddr = *((uint64_t*)&(buff[index]));
    index += 8;
    uint16_t nwAddr = *((uint16_t*)&(buff[index]));
    index += 2;

    auto dev = std::make_shared<Device>(ieeeAddr, nwAddr);

    dev->lastSeen = *((uint32_t*)&(buff[index]));
    index += 4;
    dev->interviewed = *((uint8_t*)&(buff[index++]));
    dev->type = DeviceLogicalType(*((uint8_t*)&(buff[index++])));
    dev->manufacturerCode = *((uint16_t*)&(buff[index]));
    index += 2;
    dev->powerSource = PowerSource(*((uint8_t*)&(buff[index++])));
    dev->zclVersion = *((uint8_t*)&(buff[index++]));
    dev->appVersion = *((uint8_t*)&(buff[index++]));
    dev->stackVersion = *((uint8_t*)&(buff[index++]));
    dev->hwVersion = *((uint8_t*)&(buff[index++]));

    uint8_t modelIdLen = *((uint8_t*)&(buff[index++]));
    dev->modelId = std::string((const char*)&(buff[index]), (size_t)modelIdLen);
    index += modelIdLen;

    uint8_t manufacturerLen = *((uint8_t*)&(buff[index++]));
    dev->manufacturer = std::string((const char*)&(buff[index]), (size_t)manufacturerLen);
    index += manufacturerLen;

    uint8_t swDateCodeLen = *((uint8_t*)&(buff[index++]));
    dev->swDateCode = std::string((const char*)&(buff[index]), (size_t)swDateCodeLen);
    index += swDateCodeLen;

    uint8_t swBuildIdLen = *((uint8_t*)&(buff[index++]));
    dev->swBuildId = std::string((const char*)&(buff[index]), (size_t)swBuildIdLen);
    index += swBuildIdLen;

    uint8_t numEndpoints = *((uint8_t*)&(buff[index++]));
    for(int i = 0; i < numEndpoints; i++) {
        uint16_t profileId = *((uint16_t*)&(buff[index]));
        index += 2;
        uint16_t deviceId = *((uint16_t*)&(buff[index]));
        index += 2;
        uint8_t endpoint = *((uint8_t*)&(buff[index++]));
        uint8_t numInClusters = *((uint8_t*)&(buff[index++]));
        uint8_t numOutClusters = *((uint8_t*)&(buff[index++]));

        auto inClusters = (numInClusters > 0) ? std::make_unique<uint16_t[]>(numInClusters) : std::unique_ptr<uint16_t[]>();
        auto outClusters = (numOutClusters > 0) ? std::make_unique<uint16_t[]>(numOutClusters) : std::unique_ptr<uint16_t[]>();
        if(numInClusters > 0) {
            memcpy(inClusters.get(), &(buff[index]), numInClusters*2);
            index += numInClusters*2;
        }
        if(numOutClusters > 0) {
            memcpy(outClusters.get(), &(buff[index]), numOutClusters*2);
            index += numOutClusters*2;
        }

        dev->endpoints.push_back(Endpoint(ieeeAddr, nwAddr, profileId, deviceId, endpoint, numInClusters, std::move(inClusters), numOutClusters, std::move(outClusters)));
    }

    dev->printInfo();
    return dev;
}

void Device::printInfo() {
    char name[12];
    if(!ieeeToStr(ieeeAddr, name, 12)) {
        throw std::runtime_error("Could not convert ieeeAddr");
    }
    std::cout << "Printing info for device " << name << std::endl << std::flush;
    printf("ieeeAddr: %llx\r\n", ieeeAddr);
    printf("nwAddr: %x\r\n", networkAddress);
    printf("lastSeen: %lu\r\n", lastSeen);
    printf("interviewed: %u\r\n", interviewed);
    printf("type: %u\r\n", (uint8_t)type);
    printf("manufacturerCode: %x\r\n", manufacturerCode);
    printf("powerSource: %u\r\n", (uint8_t) powerSource);
    printf("zclVersion: %u\r\n", zclVersion);
    printf("appVersion: %u\r\n", appVersion);
    printf("stackVersion: %u\r\n", stackVersion);
    printf("hwVersion: %u\r\n", hwVersion);

    printf("modelId: %s\r\n", modelId.c_str());
    printf("manufacturer: %s\r\n", manufacturer.c_str());
    printf("swDateCode: %s\r\n", swDateCode.c_str());
    printf("swBuildId: %s\r\n", swBuildId.c_str());

    printf("numEndpoints: %u\r\n", endpoints.size());
}