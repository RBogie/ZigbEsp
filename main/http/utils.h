#pragma once

#include "libesphttpd/httpd-freertos.h"
#include "cJSON.h"

void jsonResponseCommon(HttpdConnData *connData, cJSON *jsroot);