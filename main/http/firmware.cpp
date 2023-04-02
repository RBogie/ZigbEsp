#include "firmware.h"
#include "utils.h"

#include "cJSON.h"

#include "esp_flash.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include "esp_flash_partitions.h"
#include "esp_image_format.h"

#define FLST_START 0
#define FLST_WRITE 1
#define FLST_SKIP 2
#define FLST_DONE 3
#define FLST_ERROR 4

#define FILETYPE_ESPFS 0
#define FILETYPE_FLASH 1
#define FILETYPE_OTA 2

static const char *TAG = "ota";

typedef struct {
	esp_ota_handle_t update_handle;
	const esp_partition_t *update_partition;
	const esp_partition_t *configured;
	const esp_partition_t *running;
	int state;
	int filetype;
	int flashPos;
	char pageData[4096];
	int pagePos;
	int address;
	int len;
	int skip;
	const char *err;
} UploadState;

// Check that the header of the firmware blob looks like actual firmware...
static int ICACHE_FLASH_ATTR checkBinHeader(void *buf) {
	uint8_t *cd = (uint8_t *)buf;
	printf("checkBinHeader: %x %x %x\n", cd[0], ((uint16_t *)buf)[3], ((uint32_t *)buf)[0x6]);
	if (cd[0] != 0xE9) return 0;
	if (((uint16_t *)buf)[3] != 0x4008) return 0;
	uint32_t a=((uint32_t *)buf)[0x6];
	if (a!=0 && (a<=0x3F000000 || a>0x40400000)) return 0;
	return 1;
}

static int ICACHE_FLASH_ATTR checkEspfsHeader(void *buf) {
	if (memcmp(buf, "ESfs", 4)!=0) return 0;
	return 1;
}


static uint8_t verifiedStatus[3] = {0};

CgiStatus ICACHE_FLASH_ATTR getFlashInfo(HttpdConnData *connData) {
	if (connData->isConnectionClosed) {
		return HTTPD_CGI_DONE;
	}

	cJSON *jsroot = cJSON_CreateObject();

    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    const esp_partition_t *boot_partition = esp_ota_get_boot_partition();
    if (boot_partition == NULL) {boot_partition = running_partition;} // If no ota_data partition, then esp_ota_get_boot_partition() might return NULL.
    cJSON *jsapps = cJSON_AddArrayToObject(jsroot, "app");
    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, nullptr);
    uint8_t firmwareIndex = 0;
    while (it != NULL) {
        const esp_partition_t *it_partition = esp_partition_get(it);
        if (it_partition != NULL) {
            cJSON *partj = NULL;
            partj = cJSON_CreateObject();
            cJSON_AddStringToObject(partj, "name", it_partition->label);
            cJSON_AddNumberToObject(partj, "size", it_partition->size);
            esp_app_desc_t partDesc;
            if(esp_ota_get_partition_description(it_partition, &partDesc) == ESP_OK) {
                cJSON_AddStringToObject(partj, "version", partDesc.version);
            } else {
                cJSON_AddStringToObject(partj, "version", "Unknown");
            }
            if(it_partition==running_partition) {
                //We wouldn't be running if this one wasn't valid
                cJSON_AddBoolToObject(partj, "verified", true);
                cJSON_AddBoolToObject(partj, "valid", true);
            } else {
                cJSON_AddBoolToObject(partj, "verified", verifiedStatus[firmwareIndex] > 0);
                if(verifiedStatus[firmwareIndex] > 0) {
                    cJSON_AddBoolToObject(partj, "valid", (bool)(verifiedStatus[firmwareIndex] - 1));
                }
            }
            cJSON_AddBoolToObject(partj, "running", (it_partition==running_partition));
            cJSON_AddBoolToObject(partj, "bootset", (it_partition==boot_partition));
            cJSON_AddItemToArray(jsapps, partj);
            it = esp_partition_next(it);
            firmwareIndex++;
        }
    }
    esp_partition_iterator_release(it);

    cJSON *jsdatas = cJSON_AddArrayToObject(jsroot, "data");
    it = esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, nullptr);
    while (it != NULL) {
        const esp_partition_t *it_partition = esp_partition_get(it);
        if (it_partition != NULL)
        {
            cJSON *partj = NULL;
            partj = cJSON_CreateObject();
            cJSON_AddStringToObject(partj, "name", it_partition->label);
            cJSON_AddNumberToObject(partj, "size", it_partition->size);
            cJSON_AddNumberToObject(partj, "format", it_partition->subtype);
            cJSON_AddItemToArray(jsdatas, partj);
            it = esp_partition_next(it);
        }
    }
    esp_partition_iterator_release(it);
	cJSON_AddBoolToObject(jsroot, "success", true);
	jsonResponseCommon(connData, jsroot);
	return HTTPD_CGI_DONE;
}

CgiStatus ICACHE_FLASH_ATTR getFlashInfoVerified(HttpdConnData *connData) {
	if (connData->isConnectionClosed) {
		return HTTPD_CGI_DONE;
	}

    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, nullptr);
    uint8_t firmwareIndex = 0;
    while (it != NULL) {
        const esp_partition_t *it_partition = esp_partition_get(it);
        if (it_partition != NULL) {
            if(it_partition != running_partition) {
                esp_image_metadata_t data;
                const esp_partition_pos_t part_pos = {
                    .offset = it_partition->address,
                    .size = it_partition->size,
                };
                if (esp_image_verify(ESP_IMAGE_VERIFY_SILENT, &part_pos, &data) != ESP_OK) {
                    verifiedStatus[firmwareIndex] = 1;
                } else {
                    verifiedStatus[firmwareIndex] = 2;
                }
            }
            it = esp_partition_next(it);
            firmwareIndex++;
        }
    }
    esp_partition_iterator_release(it);

	return getFlashInfo(connData);
}

void setVerifiedStatus(const esp_partition_t* part, uint8_t status) {
    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, nullptr);
    uint8_t firmwareIndex = 0;
    while (it != NULL) {
        const esp_partition_t *it_partition = esp_partition_get(it);
        if (it_partition != NULL) {
            if(it_partition == part) {
                verifiedStatus[firmwareIndex] = status;
            }
            it = esp_partition_next(it);
            firmwareIndex++;
        }
    }
    esp_partition_iterator_release(it);
}

CgiStatus ICACHE_FLASH_ATTR uploadFirmware(HttpdConnData *connData) {
	UploadState *state=(UploadState *)connData->cgiData;
	esp_err_t err;

	if (connData->isConnectionClosed) {
		//Connection aborted. Clean up.
		if (state!=NULL) free(state);
		return HTTPD_CGI_DONE;
	}

	if (state == NULL) {
		//First call. Allocate and initialize state variable.
		ESP_LOGD(TAG, "Firmware upload cgi start");
		state = new UploadState();
		if (state==NULL) {
			ESP_LOGE(TAG, "Can't allocate firmware upload struct");
			return HTTPD_CGI_DONE;
		}
		memset(state, 0, sizeof(UploadState));

		state->configured = esp_ota_get_boot_partition();
		state->running = esp_ota_get_running_partition();

		// check that ota support is enabled
		if(!state->configured || !state->running)
		{
			ESP_LOGE(TAG, "configured or running parititon is null, is OTA support enabled in build configuration?");
			state->state=FLST_ERROR;
			state->err="Partition error, OTA not supported?";
		} else {
			if (state->configured != state->running) {
				ESP_LOGW(TAG, "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x",
					state->configured->address, state->running->address);
				ESP_LOGW(TAG, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
			}
			ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08x)",
				state->running->type, state->running->subtype, state->running->address);

			state->state=FLST_START;
			state->err="Premature end";
		}
        state->update_partition = esp_ota_get_next_update_partition(NULL);
	    if (state->update_partition == NULL)
	    {
			ESP_LOGE(TAG, "update_partition not found!");
			state->err="update_partition not found!";
			state->state=FLST_ERROR;
	    }

		connData->cgiData=state;
	}

	char *data = connData->post.buff;
	int dataLen = connData->post.buffLen;

	while (dataLen!=0) {
		if (state->state==FLST_START) {
			//First call. Assume the header of whatever we're uploading already is in the POST buffer.
			if (checkBinHeader(connData->post.buff)) {
			    if (state->update_partition == NULL)
			    {
					ESP_LOGE(TAG, "update_partition not found!");
					state->err="update_partition not found!";
					state->state=FLST_ERROR;
			    }
			    else
			    {
			    	ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x", state->update_partition->subtype, state->update_partition->address);
                    err = esp_ota_begin(state->update_partition, OTA_SIZE_UNKNOWN, &state->update_handle);

					if (err != ESP_OK)
					{
						ESP_LOGE(TAG, "esp_ota_begin failed, error=%d", err);
						state->err="esp_ota_begin failed!";
						state->state=FLST_ERROR;
					}
					else
					{
						ESP_LOGI(TAG, "esp_ota_begin succeeded");
						state->state = FLST_WRITE;
						state->len = connData->post.len;
					}
			    }
			} else {
				state->err="Invalid flash image type!";
				state->state=FLST_ERROR;
				ESP_LOGE(TAG, "Did not recognize flash image type");
			}
		} else if (state->state==FLST_WRITE) {
			err = esp_ota_write(state->update_handle, data, dataLen);
			if (err != ESP_OK) {
				ESP_LOGE(TAG, "Error: esp_ota_write failed! err=0x%x", err);
				state->err="Error: esp_ota_write failed!";
				state->state=FLST_ERROR;
			}

			state->len-=dataLen;
			state->address+=dataLen;
			if (state->len==0) {
				state->state=FLST_DONE;
			}

			dataLen = 0;
		} else if (state->state==FLST_DONE) {
			ESP_LOGE(TAG, "%d bogus bytes received after data received", dataLen);
			//Ignore those bytes.
			dataLen=0;
		} else if (state->state==FLST_ERROR) {
			//Just eat up any bytes we receive.
			dataLen=0;
		}
	}

	if  (connData->post.len == connData->post.received) {
		cJSON *jsroot = cJSON_CreateObject();
		if (state->state==FLST_DONE) {
			if (esp_ota_end(state->update_handle) != ESP_OK) {
				state->err="esp_ota_end failed!";
		        ESP_LOGE(TAG, "esp_ota_end failed!");
		        state->state=FLST_ERROR;
                setVerifiedStatus(state->update_partition, 1);
		    }
			else
			{
				state->err="Flash Success.";
				ESP_LOGI(TAG, "Upload done. Sending response");
                setVerifiedStatus(state->update_partition, 2);
			}
			// todo: automatically set boot flag?
			err = esp_ota_set_boot_partition(state->update_partition);
			if (err != ESP_OK) {
				ESP_LOGE(TAG, "esp_ota_set_boot_partition failed! err=0x%x", err);
			}
		}
		cJSON_AddStringToObject(jsroot, "message", state->err);
		cJSON_AddBoolToObject(jsroot, "success", (state->state==FLST_DONE)?true:false);
		delete state;

		jsonResponseCommon(connData, jsroot); // Send the json response!
		return HTTPD_CGI_DONE;
	}

	return HTTPD_CGI_MORE;
}