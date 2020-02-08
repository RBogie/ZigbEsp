#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"

#include "z-stack/Controller.h"

#include <iostream>
#include <stdexcept>

#include "nvs_flash.h"
#include "espfs_image.h"
#include "espfs.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "libesphttpd/httpd-freertos.h"
#include "libesphttpd/httpd-espfs.h"
#include "libesphttpd/route.h"
#include "cJSON.h"

static const char *TAG = "main";

#define MAX_CONNECTIONS 32u
static char connectionMemory[sizeof(RtosConnType) * MAX_CONNECTIONS];
static HttpdFreertosInstance httpdFreertosInstance;

Controller controller;

static bool state = true;

CgiStatus ICACHE_FLASH_ATTR cgiGetSetState(HttpdConnData *connData) {
	if (connData->isConnectionClosed) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}

    if(connData->requestType == HTTPD_METHOD_POST) {
        cJSON* req = cJSON_Parse(connData->post.buff);
        if(cJSON_HasObjectItem(req, "toggle")) {
            cJSON* toggle = cJSON_GetObjectItem(req, "toggle");
            if(cJSON_IsTrue(toggle)) {
                state = !state;

                for(auto& dev : controller.devices) {
                    std::cout << "Got device" << std::endl;
                    auto endpoint = dev->getEndpoint(ClusterId::genOnOff);
                    if(endpoint) {
                        if(state) {
                            std::cout << "Turning device on" << std::endl;
                            endpoint->command(controller.getZstack(), ClusterId::genOnOff, ZclCommandId::GEN_ONOFF_ON, nullptr, 0);
                        } else {
                            std::cout << "Turning device off" << std::endl;
                            endpoint->command(controller.getZstack(), ClusterId::genOnOff, ZclCommandId::GEN_ONOFF_OFF, nullptr, 0);
                        }
                    } else {
                        std::cout << "Device has no On/Off cluster" << std::endl;
                    }
                }
            }
        }
        cJSON_Delete(req);
    }

	cJSON *jsroot = cJSON_CreateObject();
    cJSON_AddBoolToObject(jsroot, "state", state);

	//// Generate the header
	//We want the header to start with HTTP code 200, which means the document is found.
	httpdStartResponse(connData, 200);
	httpdHeader(connData, "Cache-Control", "no-store, must-revalidate, no-cache, max-age=0");
	httpdHeader(connData, "Expires", "Mon, 01 Jan 1990 00:00:00 GMT");  //  This one might be redundant, since modern browsers look for "Cache-Control".
	httpdHeader(connData, "Content-Type", "application/json; charset=utf-8"); //We are going to send some JSON.
	httpdEndHeaders(connData);
	char *json_string = cJSON_Print(jsroot);
    if (json_string)
    {
    	httpdSend(connData, json_string, -1);
        cJSON_free(json_string);
    }
    cJSON_Delete(jsroot);
	return HTTPD_CGI_DONE;
}


HttpdBuiltInUrl builtInUrls[] = {
    ROUTE_CGI("/state", cgiGetSetState),
	ROUTE_FILESYSTEM(),
	ROUTE_END()
};

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "retry to connect to the AP");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    }
}

extern "C" void app_main()
{
    printf("Hello world!\n");

    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is ESP32 chip with %d CPU cores, WiFi%s%s, ",
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    printf("silicon revision %d, ", chip_info.revision);

    printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    nvs_flash_init();

    EspFsConfig espfsConf = {
        .memAddr = &espfs_image_bin,
        .partLabel = nullptr
    };
    EspFs* espfs = espFsInit(&espfsConf);
    httpdRegisterEspfs(espfs);

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

	httpdFreertosInit(&httpdFreertosInstance,
	                  builtInUrls,
	                  80,
	                  connectionMemory,
	                  MAX_CONNECTIONS,
	                  HTTPD_FLAG_NONE);
	httpdFreertosStart(&httpdFreertosInstance);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {0};
    memcpy(wifi_config.sta.ssid, "SSID", 20);
    memcpy(wifi_config.sta.password, PASSWD", 17);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    try {
        controller.start();
    } catch(std::runtime_error& e) {
        std::cout << "Error occured: " << e.what() << std::endl;
    }

    vTaskDelete( NULL );
}