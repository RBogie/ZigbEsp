#include "devices.h"
#include "utils.h"

#include "cJSON.h"

#include "../z-stack/Controller.h"

CgiStatus getDevices(HttpdConnData *connData) {
    if(connData->isConnectionClosed)
        return HTTPD_CGI_DONE;
    
    cJSON* jroot = cJSON_CreateArray();

    Controller* controller = (Controller*) connData->cgiArg;

    auto devices = controller->getDevices();
    for(auto& dev : devices) {
        cJSON* devJson = cJSON_CreateObject();

        auto ieeeStr = dev->getIeeeAddrStr();
        cJSON_AddStringToObject(devJson, "ieee", ieeeStr.c_str());
        cJSON_AddNumberToObject(devJson, "nwAddress", dev->getNetworkAddress());
        cJSON_AddStringToObject(devJson, "modelId", dev->getModelId().c_str());
        cJSON_AddStringToObject(devJson, "manufacturer", dev->getManufacturer().c_str());
        cJSON_AddNumberToObject(devJson, "lastSeen", dev->getLastSeen());

        auto endpointsArr = cJSON_CreateArray();
        auto endpoints = dev->getEndpoints();
        for(auto& endpoint : endpoints) {
            auto endpointId = endpoint->getEndpoint();
            cJSON* endp = cJSON_CreateObject();
            cJSON_AddNumberToObject(endp, "endpoint", endpointId);
            cJSON_AddItemToArray(endpointsArr, endp);
        }
        cJSON_AddItemToObject(jroot, "endpoints", endpointsArr);
        cJSON_AddItemToArray(jroot, devJson);
    }

    jsonResponseCommon(connData, jroot);
    return HTTPD_CGI_DONE;
}