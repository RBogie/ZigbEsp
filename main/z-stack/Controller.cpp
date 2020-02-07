//
// Created by Rob Bogie on 31/01/2020.
//

#include "Controller.h"

#include <iostream>
#include "nvs.h"

static nvs_handle configHandle;
static nvs_handle deviceStorageHandle;

Controller::Controller() : zstack(std::bind(&Controller::onDeviceJoin,this, std::placeholders::_1),
                                  std::bind(&Controller::onDeviceAnnounced,this, std::placeholders::_1),
                                  std::bind(&Controller::onDeviceLeft,this, std::placeholders::_1)){

}

void Controller::start() {
    esp_err_t err = nvs_open("config", NVS_READWRITE, &configHandle);
    if(err != ESP_OK)
        throw std::runtime_error("Could not open nvs config");
    err = nvs_open("devices", NVS_READWRITE, &deviceStorageHandle);
    if(err != ESP_OK)
        throw std::runtime_error("Could not open nvs device storage");

    uint32_t deviceStorageVersion = 0; // value will default to 0, if not set yet in NVS
    err = nvs_get_u32(configHandle, "deviceStorageVersion", &deviceStorageVersion);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
        throw std::runtime_error("Could not open nvs device storage");

    if(err == ESP_ERR_NVS_NOT_FOUND) {
        // No upgrades needed, just store our version
        nvs_set_u32(configHandle, "deviceStorageVersion", deviceStorageVersion);
        nvs_commit(configHandle);
    }

    auto it = nvs_entry_find(NVS_DEFAULT_PART_NAME, "devices", NVS_TYPE_BLOB);
    while(it != nullptr) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);

        std::cout << "Found device " << info.key << std::endl;

        size_t length;
        esp_err_t e = nvs_get_blob(deviceStorageHandle, info.key, nullptr, &length);
        if(e != ESP_OK && e != ESP_ERR_NVS_INVALID_LENGTH) {

            it = nvs_entry_next(it);
            continue;
        }

        uint8_t* buff = new uint8_t[length];
        if(!buff) {
            throw std::runtime_error("Could not allocate enough memory to load device");
        }
        e = nvs_get_blob(deviceStorageHandle, info.key, buff, &length);
        if(e != ESP_OK) {
            throw std::runtime_error("Could not retrieve blob from nvs for device");
        }

        devices.push_back(Device::restoreFromStorage(buff, length));
        std::cout << "Loaded device " << info.key << std::endl;
        delete[] buff;

        it = nvs_entry_next(it);
    }

    auto startStatus = zstack.start();
    std::cout << "startStatus: " << (int)startStatus << std::endl;
}

ZStack* Controller::getZstack() {
    return &zstack;
}

bool Controller::isJoingingDeviceAllowed(const uint64_t &ieeeAddr) {
    return true; // TODO: Implement logic for determining whether a device is allowed (timebased, with a blacklist)
}

void Controller::onDeviceJoin(const ZdoDeviceIndReq *const devInfo) {
    if(!isJoingingDeviceAllowed(devInfo->extaddr)) {
        // TODO: disconnect device
        return;
    }

    auto device = findDeviceByIeee(devInfo->extaddr);
    if(!device) {
        std::cout << "New device joined" << std::endl;
        device = std::make_shared<Device>(devInfo->extaddr, devInfo->nwkaddr);
        devices.push_back(device);
    } else if(device->getNetworkAddress() != devInfo->nwkaddr) {
        std::cout << "Found existing device with same ieee but different network address. Updating address" << std::endl;
        device->setNetworkAddress(devInfo->nwkaddr);
    }

    device->updateLastSeen();

    if(!device->isInterviewed() && !device->isInterviewInProgress()) {
        std::cout << "Interviewing device" << std::endl;
        device->interview(zstack, [this](Device* dev) {
            dev->saveToStorage(deviceStorageHandle);
            nvs_commit(deviceStorageHandle);
        });
        
    } else {
        std::cout << "Device already interviewed" << std::endl;
    }
}

void Controller::onDeviceAnnounced(const ZdoDeviceAnnceIndReq *const devInfo) {
    std::cout << "Device announce" << std::endl;

    auto device = findDeviceByIeee(devInfo->ieeeaddr);
    if(!device) {
        std::cout << "Device announce is from unknown device" << std::endl;
        return;
    }

    device->updateLastSeen();

    if(device->getNetworkAddress() != devInfo->nwkaddr) {
        std::cout << "Device announced with new networkAddress" << std::endl;
        device->setNetworkAddress(devInfo->nwkaddr);
    }
}

void Controller::onDeviceLeft(const ZdoLeaveIndReq *const devInfo) {
    std::cout << "Device leave" << std::endl;
    auto device = findDeviceByIeee(devInfo->extaddr);
    if(device) {
        removeDevice(device);
    }
}

std::shared_ptr<Device> Controller::findDeviceByIeee(const uint64_t &ieeeAddr) {
    for(auto dev : devices) {
        if(dev->getIeeeAddr() == ieeeAddr)
            return dev;
    }
    return std::shared_ptr<Device>();
}

void Controller::removeDevice(std::shared_ptr<Device> device) {
    auto it = devices.begin();
    while(it != devices.end()) {
        if((*it)->getIeeeAddr() == device->getIeeeAddr()) {
            *it = devices.back();
            devices.pop_back();
            return;
        }
        it++;
    }
}