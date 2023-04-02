#include "utils.h"

void jsonResponseCommon(HttpdConnData *connData, cJSON *jsroot){
	char *json = NULL;

	httpdStartResponse(connData, 200);
	httpdHeader(connData, "Cache-Control", "no-store, must-revalidate, no-cache, max-age=0");
	httpdHeader(connData, "Content-Type", "application/json; charset=utf-8");
	httpdEndHeaders(connData);
	json = cJSON_Print(jsroot);
    if (json)
    {
    	httpdSend(connData, json, -1);
        cJSON_free(json);
    }
    cJSON_Delete(jsroot);
}