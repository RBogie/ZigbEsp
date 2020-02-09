#pragma once

#include "libesphttpd/httpd-freertos.h"

CgiStatus getFlashInfo(HttpdConnData *connData);
CgiStatus getFlashInfoVerified(HttpdConnData *connData);
CgiStatus uploadFirmware(HttpdConnData *connData);
