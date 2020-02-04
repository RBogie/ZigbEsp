//
// Created by Rob Bogie on 27/01/2020.
//

#include "ZStack.h"
#include "Endpoint.h"
#include "clusters.h"

#include <iostream>
#include <functional>
#include <thread>
#include <cstring>
#include <memory>

#include "esp_pthread.h"

// TODO: Make this changeable from options
const uint8_t ZIGBEE_NETWORK_KEY[] = {0x01, 0x03, 0x05, 0x07, 0x09, 0x0B, 0x0D, 0x0F, 0x00, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0D};

const size_t endpointsLen = 10;
uint16_t ClusterId_ssIasZone = (uint8_t)ClusterId::ssIasZone;
static AfRegisterReq endpoints[] = {
        {
                .endpoint = 1,
                .appprofid = 0x0104,
                .appdeviceid = 0x0005,
                .appdevver = 0,
                .latencyreq = (uint8_t)Af::NetworkLatencyReq::NO_LATENCY_REQS,
                .appnuminclusters = 0,
                .appnumoutclusters = 0,
        },
        {
                .endpoint = 2,
                .appprofid = 0x0101,
                .appdeviceid = 0x0005,
                .appdevver = 0,
                .latencyreq = (uint8_t)Af::NetworkLatencyReq::NO_LATENCY_REQS,
                .appnuminclusters = 0,
                .appnumoutclusters = 0,
        },
        {
                .endpoint = 3,
                .appprofid = 0x0105,
                .appdeviceid = 0x0005,
                .appdevver = 0,
                .latencyreq = (uint8_t)Af::NetworkLatencyReq::NO_LATENCY_REQS,
                .appnuminclusters = 0,
                .appnumoutclusters = 0
        },
        {
                .endpoint = 4,
                .appprofid = 0x0107,
                .appdeviceid = 0x0005,
                .appdevver = 0,
                .latencyreq = (uint8_t)Af::NetworkLatencyReq::NO_LATENCY_REQS,
                .appnuminclusters = 0,
                .appnumoutclusters = 0
        },
        {
                .endpoint = 5,
                .appprofid = 0x0108,
                .appdeviceid = 0x0005,
                .appdevver = 0,
                .latencyreq = (uint8_t)Af::NetworkLatencyReq::NO_LATENCY_REQS,
                .appnuminclusters = 0,
                .appnumoutclusters = 0
        },
        {
                .endpoint = 6,
                .appprofid = 0x0109,
                .appdeviceid = 0x0005,
                .appdevver = 0,
                .latencyreq = (uint8_t)Af::NetworkLatencyReq::NO_LATENCY_REQS,
                .appnuminclusters = 0,
                .appnumoutclusters = 0
        },
        {
                .endpoint = 8,
                .appprofid = 0x0104,
                .appdeviceid = 0x0005,
                .appdevver = 0,
                .latencyreq = (uint8_t)Af::NetworkLatencyReq::NO_LATENCY_REQS,
                .appnuminclusters = 0,
                .appnumoutclusters = 0
        },
        {
                .endpoint = 11,
                .appprofid = 0x0104,
                .appdeviceid = 0x0400,
                .appdevver = 0,
                .latencyreq = (uint8_t)Af::NetworkLatencyReq::NO_LATENCY_REQS,
                .appnuminclusters = 0,
                .appnumoutclusters = 1,
                .appoutclusterlist = &ClusterId_ssIasZone
        },
        {
                .endpoint = 0x6E,
                .appprofid = 0x0104,
                .appdeviceid = 0x0005,
                .appdevver = 0,
                .latencyreq = (uint8_t)Af::NetworkLatencyReq::NO_LATENCY_REQS,
                .appnuminclusters = 0,
                .appnumoutclusters = 0
        },
        {
                .endpoint = 12,
                .appprofid = 0xc05e,
                .appdeviceid = 0x0005,
                .appdevver = 0,
                .latencyreq = (uint8_t)Af::NetworkLatencyReq::NO_LATENCY_REQS,
                .appnuminclusters = 0,
                .appnumoutclusters = 0
        }
};

ZStack::ZStack(DeviceJoinedHandler deviceJoinedHandler,
               DeviceAnnouncedHandler deviceAnnouncedHandler,
               DeviceLeftHandler deviceLeftHandler)
               :
               deviceJoinedHandler(deviceJoinedHandler),
               deviceAnnouncedHandler(deviceAnnouncedHandler),
               deviceLeftHandler(deviceLeftHandler) {

}

ZStackStartResult ZStack::start() {
    znp.setFrameHandler(std::bind(&ZStack::onFrameReceived, this, std::placeholders::_1));
    znp.start();

    getVersion();

    esp_pthread_cfg_t cfg = esp_pthread_get_default_config();
    cfg.thread_name = "zstackTaskRun";
    cfg.stack_size = (8 * 1024);
    esp_pthread_set_cfg(&cfg);

    auto returnValue = startZnp();
    std::thread taskThread(std::bind(&ZStack::runTasks, this));
    taskThread.detach();
    return returnValue;
}

NodeDescriptor ZStack::getNodeDescriptor(uint16_t networkAddr) {
    auto future = znp.waitFor(Type::AREQ, Subsystem::ZDO, CommandId::ZDO_NODE_DESC_RSP);
    ZdoNodeDescReqReq req;
    req.nwkaddrofinterest = networkAddr;
    req.dstaddr = networkAddr;
    znp.request<ZdoNodeDescReqResp>(ZnpFrame(Type::SREQ, Subsystem::ZDO, CommandId::ZDO_NODE_DESC_REQ, (uint8_t*)&req, sizeof(ZdoNodeDescReqReq)), std::chrono::milliseconds(TIMEOUT_REQUEST_SREQ));

    if(future.wait_for(std::chrono::milliseconds(TIMEOUT_REQUEST_SREQ)) != std::future_status::ready) {
        throw std::runtime_error("Timeout while retrieving node descriptor");
    }

    auto frame = future.get();
    auto data = frame.getData<ZdoNodeDescRspReq>();
    return {
        .type = DeviceLogicalType(data->logicaltype_cmplxdescavai_userdescavai & 0x07),
        .manufacturerCode = data->manufacturercode
    };
}

ZdoActiveEpRspReq ZStack::getActiveEndpoints(uint16_t networkAddr) {
    auto future = znp.waitFor(Type::AREQ, Subsystem::ZDO, CommandId::ZDO_ACTIVE_EP_RSP, [networkAddr](const ZnpFrame& f) -> bool {
        return f.getData<ZdoActiveEpRspReq>()->nwkaddr == networkAddr;
    });
    ZdoActiveEpReqReq req;
    req.dstaddr = networkAddr;
    req.nwkaddrofinterest = networkAddr;
    znp.request<ZdoActiveEpReqResp>(ZnpFrame(Type::SREQ, Subsystem::ZDO, CommandId::ZDO_ACTIVE_EP_REQ, (uint8_t*)&req, sizeof(ZdoActiveEpReqReq)), std::chrono::milliseconds(TIMEOUT_REQUEST_SREQ));
    if(future.wait_for(std::chrono::milliseconds(TIMEOUT_REQUEST_SREQ)) != std::future_status::ready) {
        throw std::runtime_error("Timeout while retrieving endpoints");
    }

    auto frame = future.get();
    return *frame.getData<ZdoActiveEpRspReq>();
}

Endpoint ZStack::getSimpleDescriptor(uint64_t ieeeAddr, uint16_t networkAddr, uint8_t endpoint) {
    auto future = znp.waitFor(Type::AREQ, Subsystem::ZDO, CommandId::ZDO_SIMPLE_DESC_RSP, [networkAddr, endpoint](const ZnpFrame& f) -> bool {
        auto data = f.getData<ZdoSimpleDescRspReq>();
        return data->nwkaddr == networkAddr && data->endpoint == endpoint;
    });
    ZdoSimpleDescReqReq req;
    req.dstaddr = networkAddr;
    req.nwkaddrofinterest = networkAddr;
    req.endpoint = endpoint;
    znp.request<ZdoSimpleDescReqResp>(ZnpFrame(Type::SREQ, Subsystem::ZDO, CommandId::ZDO_SIMPLE_DESC_REQ, (uint8_t*)&req, sizeof(ZdoSimpleDescReqReq)), std::chrono::milliseconds(TIMEOUT_REQUEST_SREQ));
    if(future.wait_for(std::chrono::milliseconds(TIMEOUT_REQUEST_SREQ)) != std::future_status::ready) {
        throw std::runtime_error("Timeout while retrieving simple descriptor");
    }

    auto frame = future.get();
    auto resp = frame.getData<ZdoSimpleDescRspReq>();

    uint8_t numInClusters = resp->clusterInfo[0];
    uint8_t numOutClusters = resp->clusterInfo[1+(numInClusters*2)];
    auto inClusters = (numInClusters > 0) ? std::make_unique<uint16_t[]>(numInClusters) : std::unique_ptr<uint16_t[]>();
    auto outClusters = (numOutClusters > 0) ? std::make_unique<uint16_t[]>(numOutClusters) : std::unique_ptr<uint16_t[]>();
    if(numInClusters > 0)
        memcpy(inClusters.get(), &(resp->clusterInfo[1]), numInClusters*2);
    if(numOutClusters > 0)
        memcpy(outClusters.get(), &(resp->clusterInfo[2+(numInClusters*2)]), numOutClusters*2);

    return Endpoint(ieeeAddr, networkAddr, resp->profileid, resp->deviceid, resp->endpoint, numInClusters, std::move(inClusters), numOutClusters, std::move(outClusters));

}

void ZStack::getVersion() {
    auto future = znp.request(ZnpFrame(Type::SREQ, Subsystem::SYS, CommandId::SYS_VERSION, nullptr, 0));
    if(!future.valid() || future.wait_for(std::chrono::milliseconds(TIMEOUT_REQUEST_SREQ)) != std::future_status::ready) {
        throw std::runtime_error("Timeout while retrieving ZNP version");
    }

    auto frame = future.get();
    version = *((const SysVersionResp*) frame.getData());

    std::cout << "ZNP version " << (int)version.product << ", revision " << version.revision << std::endl;
}

ZStackStartResult ZStack::startZnp() {
    ZStackStartResult result = ZStackStartResult::Resumed;
    bool needsConfigure;

    bool isZstack12 = version.product == (uint8_t) ZnpVersion::zStack12;

    {
        ZnpFrame respFrame = doOsalReadNv(
                isZstack12 ? NvItemsId::ZNP_HAS_CONFIGURED_ZSTACK1 : NvItemsId::ZNP_HAS_CONFIGURED_ZSTACK3, 0);
        const SysOsalNvReadResp *resp = (const SysOsalNvReadResp *) respFrame.getData();
        needsConfigure = resp->len != 1 || resp->value[0] != 0x55;
    }

    if(needsInitializing()) {
        initialise();

        if(isZstack12) {
            result = needsConfigure ? ZStackStartResult::Restored : ZStackStartResult::Reset;
        } else {
            result = ZStackStartResult::Reset;
        }
    }

    boot();
    registerEndpoints();

    if(result == ZStackStartResult::Restored) {
        //Write the channel list again because it sometimes is not saved correctly
        {
            // Channel of the device (currently 11). TODO: Make this changeable from options
            uint32_t buff = 2048;
            doOsalwriteNv(NvItemsId::CHANLIST, 0, (uint8_t*)&buff, 4);
        }
    }
    return result;
}

bool ZStack::needsInitializing() {
    bool isZstack12 = version.product == (uint8_t) ZnpVersion::zStack12;

    {
        ZnpFrame respFrame = doOsalReadNv(
                isZstack12 ? NvItemsId::ZNP_HAS_CONFIGURED_ZSTACK1 : NvItemsId::ZNP_HAS_CONFIGURED_ZSTACK3, 0);
        auto *resp = (const SysOsalNvReadResp *) respFrame.getData();
        if(resp->len != 1 || resp->value[0] != 0x55)
            return true;
    }
    {
        ZnpFrame respFrame = doOsalReadNv(NvItemsId::CHANLIST, 0);
        auto *resp = (const SysOsalNvReadResp *) respFrame.getData();
        if(resp->len != 4 || *((uint32_t*)resp->value) != 2048) {
            return true;
        }
    }
    {
        ZnpFrame respFrame = doOsalReadNv(NvItemsId::PRECFGKEYS_ENABLE, 0);
        auto *resp = (const SysOsalNvReadResp *) respFrame.getData();
        if(resp->len != 1 || resp->value[0] != 0) {
            return true;
        }
    }

    if(!isZstack12) {
        throw std::runtime_error("Zstack 3xx support is not implemented");
    } else {
        {
            //Check network key
            SapiReadConfigurationReq req;
            req.configid = (uint8_t)NvItemsId::PRECFGKEY;
            auto resp = znp.request<SapiReadConfigurationResp>(
                    ZnpFrame(Type::SREQ, Subsystem::SAPI, CommandId::SAPI_READ_CONFIGURATION, (uint8_t*)&req, sizeof(SapiReadConfigurationReq)),
                    std::chrono::milliseconds(TIMEOUT_REQUEST_SREQ)
                    );
            if(resp.len != 16)
                return true;

            for(int i = 0; i < 16; i++) {
                if(resp.value[i] != ZIGBEE_NETWORK_KEY[i])
                    return true;
            }
        }
    }

    {
        ZnpFrame respFrame = doOsalReadNv(NvItemsId::PANID, 0);
        auto *resp = (const SysOsalNvReadResp *) respFrame.getData();
        if(resp->len != 2 || *((uint16_t*)resp->value) != 0x1a62) {
            return true;
        }
    }
    {
        ZnpFrame respFrame = doOsalReadNv(NvItemsId::EXTENDED_PAN_ID, 0);
        auto *resp = (const SysOsalNvReadResp *) respFrame.getData();
        if(resp->len != 8) {
            return true;
        }
        for(int i = 0; i < 8; i++)
            if(resp->value[i] != 0xDD)
                return true;
    }

    return false;
}

void ZStack::initialise() {
    std::cout << "Initialising coordinator" << std::endl;
    bool isZstack12 = version.product == (uint8_t) ZnpVersion::zStack12;
    {
        //Reset device
        SysResetReq resetPayload;
        resetPayload.type = RESET_REQ_TYPE_SOFT;
        auto future = znp.request(ZnpFrame(Type::AREQ, Subsystem::SYS, CommandId::SYS_RESET_REQ, (uint8_t *) &resetPayload,
                                           sizeof(SysResetReq)));
        if (future.valid() &&
            future.wait_for(std::chrono::milliseconds(TIMEOUT_REQUEST_RESET)) != std::future_status::ready) {
            throw std::runtime_error("Timeout during reset");
        }
    }
    {
        // Clear device state
        uint8_t buff[] = {0x02};
        doOsalwriteNv(NvItemsId::STARTUP_OPTION, 0, buff, 1);
    }
    {
        //Reset device
        SysResetReq resetPayload;
        resetPayload.type = RESET_REQ_TYPE_SOFT;
        auto future = znp.request(ZnpFrame(Type::AREQ, Subsystem::SYS, CommandId::SYS_RESET_REQ, (uint8_t *) &resetPayload,
                                           sizeof(SysResetReq)));
        if (future.valid() &&
            future.wait_for(std::chrono::milliseconds(TIMEOUT_REQUEST_RESET)) != std::future_status::ready) {
            throw std::runtime_error("Timeout during reset");
        }
    }
    {
        // Make device coordinator
        uint8_t buff[] = {(uint8_t)DeviceLogicalType::COORDINATOR};
        doOsalwriteNv(NvItemsId::LOGICAL_TYPE, 0, buff, 1);
    }
    {
        // Set whether the device should distribute the network key. TODO: Make this an option
        uint8_t buff[] = {0};
        doOsalwriteNv(NvItemsId::PRECFGKEYS_ENABLE, 0, buff, 1);
    }
    {
        //Whether the device should always receive callbacks (verbose) or register for a specific callback. Currently verbose
        uint8_t buff[] = {1};
        doOsalwriteNv(NvItemsId::ZDO_DIRECT_CB, 0, buff, 1);
    }
    {
        // Channel of the device (currently 11). TODO: Make this changeable from options
        uint32_t buff = 2048;
        doOsalwriteNv(NvItemsId::CHANLIST, 0, (uint8_t*)&buff, 4);
    }
    {
        // PanId of the device. TODO: Make this changeable from options
        uint16_t buff = 0x1a62;
        doOsalwriteNv(NvItemsId::PANID, 0, (uint8_t*)&buff, 2);
    }
    {
        // Extended PanId of the device. TODO: Make this changeable from options
        uint8_t buff[] = {0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD};
        doOsalwriteNv(NvItemsId::EXTENDED_PAN_ID, 0, buff, 8);
    }

    if(!isZstack12) {
        throw std::runtime_error("3.0 stack is not yet implemented");
    } else {
        {
            // Set networkkey.
            SapiWriteConfigurationReq req;
            req.configid = (uint8_t)NvItemsId::PRECFGKEY; //Save since it does fit in an uint8 (98)
            req.len = 16;
            memcpy(req.value, ZIGBEE_NETWORK_KEY, 16);
            auto future = znp.request(ZnpFrame(Type::SREQ, Subsystem::SAPI, CommandId::SAPI_WRITE_CONFIGURATION, (uint8_t*)&req, 2 + req.len));
            if (future.valid() &&
                future.wait_for(std::chrono::milliseconds(TIMEOUT_REQUEST_SREQ)) != std::future_status::ready) {
                throw std::runtime_error("Timeout while setting network key");
            }
        }
        {
            // Write ZigBee Alliance Pre-configured TC Link Key - 'ZigBeeAlliance09'
            uint8_t buff[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x5a, 0x69, 0x67, 0x42, 0x65, 0x65, 0x41, 0x6c,
                              0x6c, 0x69, 0x61, 0x6e, 0x63, 0x65, 0x30, 0x39, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
            doOsalwriteNv(NvItemsId::LEGACY_TCLK_TABLE_START, 0, buff, 0x20);
        }
    }

    {
        SysOsalNvItemInitReq req;
        req.id = (uint16_t)(isZstack12 ? NvItemsId::ZNP_HAS_CONFIGURED_ZSTACK1 : NvItemsId::ZNP_HAS_CONFIGURED_ZSTACK3);
        req.len = 1;
        req.initlen = 1;
        req.initValue[0] = 0;
        auto future = znp.request(ZnpFrame(Type::SREQ, Subsystem::SYS, CommandId::SYS_OSAL_NV_ITEM_INIT, (uint8_t*)&req, 4));
        if (future.valid() &&
            future.wait_for(std::chrono::milliseconds(TIMEOUT_REQUEST_SREQ)) != std::future_status::ready) {
            throw std::runtime_error("Timeout while setting network key");
        }
        auto frame = future.get();
        auto resp = (SysOsalNvItemInitResp*)frame.getData();
        if(resp->status != 0 && resp->status != 9)
            throw std::runtime_error("OsalNvItemInit did not return correct status");
    }
    {
        uint8_t buff = 0x55;
        doOsalwriteNv(isZstack12 ? NvItemsId::ZNP_HAS_CONFIGURED_ZSTACK1 : NvItemsId::ZNP_HAS_CONFIGURED_ZSTACK3, 0, &buff, 1);
    }
}

void ZStack::boot() {
    auto result = znp.request<UtilGetDeviceInfoResp>(ZnpFrame(Type::SREQ, Subsystem::UTIL, CommandId::UTIL_GET_DEVICE_INFO, nullptr, 0), std::chrono::milliseconds(TIMEOUT_REQUEST_SREQ));
    if(DeviceState(result.devicestate) == DeviceState::ZB_COORD) {
        std::cout << "Device already initialized as coordinator" << std::endl;
        return;
    }

    std::cout << "Starting device in coordinator mode" << std::endl;
    auto started = znp.waitFor(Type::AREQ, Subsystem::ZDO, CommandId::ZDO_STATE_CHANGE_IND, [](const ZnpFrame& frame) -> bool {
        return frame.dataLen == 1 && DeviceState(frame.getData()[0]) == DeviceState::ZB_COORD;
    });
    ZdoStartupFromAppReq startReq;
    startReq.startdelay = 100;
    auto status = znp.request<ZdoStartupFromAppResp>(ZnpFrame(Type::SREQ, Subsystem::ZDO, CommandId::ZDO_STARTUP_FROM_APP, (uint8_t*)&startReq, sizeof(ZdoStartupFromAppReq)), std::chrono::milliseconds(TIMEOUT_REQUEST_SREQ)).status;
    if(status != 0 && status != 1) {
        throw std::runtime_error("ZDO_STARTUP_FROM_APP returned incorrect status");
    }
    started.wait_for(std::chrono::milliseconds(60000));
    std::cout << "Device started in coordinator mode" << std::endl;
}

void ZStack::registerEndpoints() {
    auto activeEpFrameFuture = znp.waitFor(Type::AREQ, Subsystem::ZDO, CommandId::ZDO_ACTIVE_EP_RSP);
    ZdoActiveEpReqReq req;
    req.dstaddr = 0;
    req.nwkaddrofinterest = 0;
    znp.request(ZnpFrame(Type::SREQ, Subsystem::ZDO, CommandId::ZDO_ACTIVE_EP_REQ, (uint8_t*)&req, sizeof(ZdoActiveEpReqReq)));
    if(activeEpFrameFuture.wait_for(std::chrono::milliseconds(TIMEOUT_REQUEST_SREQ)) != std::future_status::ready) {
        throw std::runtime_error("Timeout during activeEpRsp wait");
    }

    auto activeEpFrame = activeEpFrameFuture.get();
    auto activeEps = (ZdoActiveEpRspReq*)activeEpFrame.getData();

    for(int i = 0; i < endpointsLen; i++) {
        bool epRegistered = false;
        for(int j = 0; j < activeEps->activeepcount; j++) {
            if(activeEps->activeeplist[j] == endpoints[i].endpoint) {
                epRegistered = true;
                break;
            }
        }
        if(epRegistered) {
            std::cout << "Endpoint " << (int)endpoints[i].endpoint << " already registered" << std::endl;
            continue;
        }

        //Start registering this endpoint
        uint8_t buff[250];
        memcpy(buff, &(endpoints[i]), 8); //Copy fields before first list, then build correct format with lists after each length
        memcpy(buff+8, endpoints[i].appinclusterlist, 2* endpoints[i].appnuminclusters);
        buff[8 + (2 * endpoints[i].appnuminclusters)] = endpoints[i].appnumoutclusters;
        memcpy(buff + 9 + (2 * endpoints[i].appnuminclusters), endpoints[i].appoutclusterlist,2 * endpoints[i].appnumoutclusters);

        znp.request<AfRegisterResp>(
                ZnpFrame(
                        Type::SREQ,
                        Subsystem::AF,
                        CommandId::AF_REGISTER,
                        buff,
                        9 + (2 * (endpoints[i].appnumoutclusters + endpoints[i].appnuminclusters))),
                std::chrono::milliseconds(TIMEOUT_REQUEST_SREQ));
    }
}

void ZStack::onFrameReceived(const ZnpFrame &frame) {
    if(frame.type != Type::AREQ) {
        return; //We are only interested in frames that are sent to us without us asking
    }

    if(frame.subsystem == Subsystem::ZDO) {
        switch(frame.commandId) {
            case CommandId::ZDO_DEVICE_IND: {
                auto payload = frame.getData<ZdoDeviceIndReq>();
                if (deviceJoinedHandler) {
                    deviceJoinedHandler(payload);
                }
                break;
            }
            case CommandId::ZDO_DEVICE_ANNCE_IND: {
                auto payload = frame.getData<ZdoDeviceAnnceIndReq>();
                if (deviceJoinedHandler) {
                    deviceAnnouncedHandler(payload);
                }
                break;
            }
            case CommandId::ZDO_LEAVE_IND: {
                auto payload = frame.getData<ZdoLeaveIndReq>();
                if(deviceJoinedHandler) {
                    deviceLeftHandler(payload);
                }
                break;
            }
            default:
                break;
        }
    }
}

ZnpFrame ZStack::doOsalReadNv(NvItemsId id, uint8_t offset) {
    SysOsalNvReadReq req;
    req.id=(uint16_t)id;
    req.offset = offset;
    auto future = znp.request(ZnpFrame(Type::SREQ, Subsystem::SYS, CommandId::SYS_OSAL_NV_READ, (uint8_t*)&req, sizeof(SysOsalNvReadReq)));
    if(!future.valid() || future.wait_for(std::chrono::milliseconds(TIMEOUT_REQUEST_SREQ)) != std::future_status::ready) {
        throw std::runtime_error("Timeout during osalReadNv");
    }

    return future.get();
}

ZnpFrame ZStack::doOsalwriteNv(NvItemsId id, uint8_t offset, const uint8_t *buff, uint8_t length) {
    SysOsalNvWriteReq req;
    req.id=(uint16_t)id;
    req.offset = offset;
    req.len = length;
    memcpy(req.value, buff, length);

    auto future = znp.request(ZnpFrame(Type::SREQ, Subsystem::SYS, CommandId::SYS_OSAL_NV_WRITE, (uint8_t*)&req, 4 + length));
    if(!future.valid() || future.wait_for(std::chrono::milliseconds(TIMEOUT_REQUEST_SREQ)) != std::future_status::ready) {
        throw std::runtime_error("Timeout during osalWriteNv");
    }

    return future.get();
}

void ZStack::addTask(ZigbeeTask task) {
    {
        std::unique_lock<std::mutex> lk(tasksMutex);
        tasks.push(task);
    }
    tasksConditional.notify_one();
}

void ZStack::runTasks() {
    while(true) {
        ZigbeeTask task;
        {
            std::unique_lock<std::mutex> lk(tasksMutex);
            tasksConditional.wait(lk, [this]{return !tasks.empty();});

            task = tasks.front();
            tasks.pop();
        }
        task();
    }
}

uint8_t ZStack::getNextTransactionSequence() {
    transactionSequence++;

    if(transactionSequence == 0)
        transactionSequence = 1;

    return transactionSequence;
}

uint8_t ZStack::getNextTransactionId() {
    transactionId++;

    if(transactionId == 0)
        transactionId = 1;

    return transactionId;
}

AfDataConfirmReq ZStack::dataRequest(uint16_t destinationAddr, uint8_t destinationEndpoint, uint8_t srcEndpoint, ClusterId clusterId, uint8_t radius, uint8_t* data, uint8_t dataLen, uint16_t timeout) {
    auto transId = getNextTransactionId();
    auto future = znp.waitFor(Type::AREQ, Subsystem::AF, CommandId::AF_DATA_CONFIRM, [transId](const ZnpFrame& frame)->bool{
        return frame.getData<AfDataConfirmReq>()->transid == transId;
    });

    AfDataReqReq req;
    req.dstaddr = destinationAddr;
    req.destendpoint = destinationEndpoint;
    req.srcendpoint = srcEndpoint;
    req.clusterid = (uint16_t)clusterId;
    req.transid = transId;
    req.options = 0;
    req.radius = radius;
    req.len = dataLen;
    memcpy(req.data, data, dataLen);
    znp.request<AfDataReqResp>(ZnpFrame(Type::SREQ, Subsystem::AF, CommandId::AF_DATA_REQUEST, (uint8_t*)&req, 10 + req.len), std::chrono::milliseconds(TIMEOUT_REQUEST_SREQ));

    if(future.wait_for(std::chrono::milliseconds(timeout)) != std::future_status::ready) {
        throw std::runtime_error("Timeout during dataConfirm");
    }

    auto frame = future.get();
    auto resp = frame.getData<AfDataConfirmReq>();
    if(resp->status != 0) {
        throw std::runtime_error("dataConfirm error. Status not 0");
    }

    return *resp;
}

ZclFrame ZStack::sendZclFrameNetworkAddressWithResponse(uint16_t nwAddr, uint8_t endpoint, const ZclFrame& frame, uint16_t timeout) {
    //TODO: wait for default response

    auto future = znp.waitFor(Type::AREQ, Subsystem::AF, CommandId::AF_INCOMING_MESSAGE, [nwAddr, endpoint, &frame](const ZnpFrame& znpFrame) -> bool {
        auto msgPayload = znpFrame.getData<AfIncomingMsgReq>();
        if( msgPayload->srcaddr == nwAddr &&
            msgPayload->srcendpoint == endpoint) {

            ZclFrame zclFrame = ZclFrame::fromBuffer(ClusterId(msgPayload->clusterid), msgPayload->data, msgPayload->len);
            return zclFrame.getTransactionSequence() == frame.getTransactionSequence()
                   && zclFrame.getClusterId() == frame.getClusterId()
                   && zclFrame.getType() == frame.getType()
                   && zclFrame.getDirection() == Direction::SERVER_TO_CLIENT
                   && zclFrame.getCommandId() == ZclCommandId::GLOBAL_READ_RSP;
        }
        return false;
    });

    uint8_t buffer[250];
    uint8_t actualSize = frame.toBuffer(buffer, 250);
    dataRequest(nwAddr, endpoint, 1, frame.getClusterId(), Af::DEFAULT_RADIUS, buffer, actualSize, timeout);

    if(future.wait_for(std::chrono::milliseconds(timeout)) != std::future_status::ready) {
        throw std::runtime_error("Timeout waiting for ZclFrame GlobalReadRsp");
    }

    auto rspFrame = future.get();
    auto zclResp = rspFrame.getData<AfIncomingMsgReq>();
    return ZclFrame::fromBuffer(ClusterId(zclResp->clusterid), zclResp->data, zclResp->len);
}

void ZStack::sendZclFrameNetworkAddress(uint16_t nwAddr, uint8_t endpoint, const ZclFrame& frame, uint16_t timeout) {
    //TODO: wait for default response

    uint8_t buffer[250];
    uint8_t actualSize = frame.toBuffer(buffer, 250);
    dataRequest(nwAddr, endpoint, 1, frame.getClusterId(), Af::DEFAULT_RADIUS, buffer, actualSize, timeout);
}

