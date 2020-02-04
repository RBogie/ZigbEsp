//
// Created by Rob Bogie on 31/01/2020.
//

#include "Controller.h"

#include <iostream>

Controller::Controller() : zstack(std::bind(&Controller::onDeviceJoin,this, std::placeholders::_1),
                                  std::bind(&Controller::onDeviceAnnounced,this, std::placeholders::_1),
                                  std::bind(&Controller::onDeviceLeft,this, std::placeholders::_1)){

}

void Controller::start() {
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
        device->interview(zstack);
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