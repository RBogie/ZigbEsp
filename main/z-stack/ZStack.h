#pragma once

#include "znp.hpp"
#include "ZclFrame.h"

#include <functional>
#include <queue>

enum class ZStackStartResult : uint8_t {
    Reset = 0,
    Resumed = 1,
    Restored = 2
};

class Endpoint;

class ZStack {
public:
    typedef std::function<void(void)> ZigbeeTask;
    typedef std::function<void(const ZdoDeviceIndReq* const)> DeviceJoinedHandler;
    typedef std::function<void(const ZdoDeviceAnnceIndReq* const)> DeviceAnnouncedHandler;
    typedef std::function<void(const ZdoLeaveIndReq* const)> DeviceLeftHandler;

    ZStack( DeviceJoinedHandler deviceJoinedHandler,
            DeviceAnnouncedHandler deviceAnnouncedHandler,
            DeviceLeftHandler deviceLeftHandler);
    ZStackStartResult start();
    void addTask(ZigbeeTask task);

    NodeDescriptor getNodeDescriptor(uint16_t networkAddr);
    ZdoActiveEpRspReq getActiveEndpoints(uint16_t networkAddr);
    Endpoint getSimpleDescriptor(uint64_t ieeeAddr, uint16_t networkAddr, uint8_t endpoint);

    uint8_t getNextTransactionSequence();
    ZclFrame sendZclFrameNetworkAddressWithResponse(uint16_t nwAddr, uint8_t endpoint, const ZclFrame& frame, uint16_t timeout);
    void sendZclFrameNetworkAddress(uint16_t nwAddr, uint8_t endpoint, const ZclFrame& frame, uint16_t timeout);
private:
    DeviceJoinedHandler deviceJoinedHandler;
    DeviceAnnouncedHandler deviceAnnouncedHandler;
    DeviceLeftHandler deviceLeftHandler;

    std::queue<ZigbeeTask> tasks;
    std::mutex tasksMutex;
    std::condition_variable tasksConditional;

    ZNP znp;
    SysVersionResp version;

    uint8_t transactionSequence = 1;
    uint8_t transactionId = 1;

    void runTasks();
    void getVersion();
    ZStackStartResult startZnp();
    bool needsInitializing();
    void initialise();
    void boot();
    void registerEndpoints();

    void onFrameReceived(const ZnpFrame& frame);

    ZnpFrame doOsalReadNv(NvItemsId id, uint8_t offset);
    ZnpFrame doOsalwriteNv(NvItemsId id, uint8_t offset, const uint8_t* buff, uint8_t length);

    AfDataConfirmReq dataRequest(uint16_t destinationAddr, uint8_t destinationEndpoint, uint8_t srcEndpoint, ClusterId clusterId, uint8_t radius, uint8_t* data, uint8_t dataLen, uint16_t timeout);

    uint8_t getNextTransactionId();
};
