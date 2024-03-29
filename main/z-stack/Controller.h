#pragma once

#include <vector>
#include "Device.h"
#include "ZStack.h"

class Controller {
public:
    Controller();
    void start();
    ZStack* getZstack();
    std::vector<std::shared_ptr<Device>>& getDevices();
private:
    ZStack zstack;
    std::vector<std::shared_ptr<Device>> devices;

    bool isJoingingDeviceAllowed(const uint64_t& ieeeAddr);

    void onDeviceJoin(const ZdoDeviceIndReq* const);
    void onDeviceAnnounced(const ZdoDeviceAnnceIndReq* const);
    void onDeviceLeft(const ZdoLeaveIndReq* const);

    std::shared_ptr<Device> findDeviceByIeee(const uint64_t& ieeeAddr);
    void removeDevice(std::shared_ptr<Device> device);
};