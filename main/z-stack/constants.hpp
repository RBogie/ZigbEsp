#pragma once

#include <cstddef>
#include <cstdint>

const uint8_t SOF = 0xFE; //Start of frame

const size_t TIMEOUT_REQUEST_SREQ = 6000;
const size_t TIMEOUT_REQUEST_RESET = 30000;

const uint8_t RESET_REQ_TYPE_HARD = 0;
const uint8_t RESET_REQ_TYPE_SOFT = 1;

enum class ZnpVersion : uint8_t {
    zStack12 = 0,
    zStack3x0 = 1,
    zStack30x = 2,
};


enum class Type : uint8_t {
    POLL = 0,
    SREQ = 1,
    AREQ = 2,
    SRSP = 3
};

enum class Subsystem : uint8_t {
    RESERVED = 0,
    SYS = 1,
    MAC = 2,
    NWK = 3,
    AF = 4,
    ZDO = 5,
    SAPI = 6,
    UTIL = 7,
    DEBUG = 8,
    APP = 9,
    APP_CNF = 15,
    GREENPOWER = 21
};

enum class CommandId : uint8_t {
    AF_REGISTER = 0,
    AF_DATA_REQUEST = 1,
    AF_DATA_CONFIRM = 128,
    AF_INCOMING_MESSAGE = 129,

    SYS_RESET_REQ = 0,
    SYS_VERSION = 2,
    SYS_OSAL_NV_ITEM_INIT = 7,
    SYS_OSAL_NV_READ = 8,
    SYS_OSAL_NV_WRITE = 9,
    SYS_RESET_IND = 128,

    SAPI_READ_CONFIGURATION = 4,
    SAPI_WRITE_CONFIGURATION = 5,

    UTIL_GET_DEVICE_INFO = 0,

    ZDO_NODE_DESC_REQ = 2,
    ZDO_SIMPLE_DESC_REQ = 4,
    ZDO_ACTIVE_EP_REQ = 5,
    ZDO_STARTUP_FROM_APP = 64,
    ZDO_NODE_DESC_RSP = 130,
    ZDO_SIMPLE_DESC_RSP = 132,
    ZDO_ACTIVE_EP_RSP = 133,
    ZDO_STATE_CHANGE_IND = 192,
    ZDO_DEVICE_IND = 202,
    ZDO_DEVICE_ANNCE_IND = 193,
    ZDO_LEAVE_IND = 201,
};

enum class NvItemsId: uint16_t {
    ZNP_HAS_CONFIGURED_ZSTACK1 = 3840,
    ZNP_HAS_CONFIGURED_ZSTACK3 = 96,
    STARTUP_OPTION = 3,
    LOGICAL_TYPE = 135,
    PRECFGKEYS_ENABLE = 99,
    ZDO_DIRECT_CB = 143,
    PANID = 131,
    CHANLIST = 132,
    EXTENDED_PAN_ID = 45,
    LEGACY_TCLK_TABLE_START = 257,
    PRECFGKEY = 98
};

enum class DeviceLogicalType : uint8_t {
    COORDINATOR= 0,
    ROUTER = 1,
    ENDDEVICE = 2,
    COMPLEX_DESC_AVAIL = 4,
    USER_DESC_AVAIL = 8,
    RESERVED1 = 16,
    RESERVED2 = 32,
    RESERVED3 = 64,
    RESERVED4 = 128
};

enum class DeviceState: uint8_t {
    HOLD = 0,
    INIT = 1,
    NWK_DISC = 2,
    NWK_JOINING = 3,
    NWK_REJOIN = 4,
    END_DEVICE_UNAUTH = 5,
    END_DEVICE = 6,
    ROUTER = 7,
    COORD_STARTING = 8,
    ZB_COORD = 9,
    NWK_ORPHAN = 10,
    INVALID_REQTYPE = 128,
    DEVICE_NOT_FOUND = 129,
    INVALID_EP = 130,
    NOT_ACTIVE = 131,
    NOT_SUPPORTED = 132,
    TIMEOUT = 133,
    NO_MATCH = 134,
    NO_ENTRY = 136,
    NO_DESCRIPTOR = 137,
    INSUFFICIENT_SPACE = 138,
    NOT_PERMITTED = 139,
    TABLE_FULL = 140,
    NOT_AUTHORIZED = 141,
    BINDING_TABLE_FULL = 142
};

class Af {
public:
    enum class NetworkLatencyReq : uint8_t{
        NO_LATENCY_REQS = 0,
        FAST_BEACONS = 1,
        SLOW_BEACONS = 2
    };

    static const uint8_t BEACON_MAX_DEPTH = 0x0F;
    static const uint16_t DEFAULT_RADIUS = (2 * BEACON_MAX_DEPTH);
};

struct NodeDescriptor {
    DeviceLogicalType type;
    uint16_t manufacturerCode;
};

/////////////////////////////////////////
/// Parameter Structures for commands ///
/////////////////////////////////////////

typedef struct __attribute__ ((packed)){
    uint8_t endpoint;
    uint16_t appprofid;
    uint16_t appdeviceid;
    uint8_t appdevver;
    uint8_t latencyreq;
    uint8_t appnuminclusters;
    uint8_t appnumoutclusters;
    uint16_t* appoutclusterlist; //The actual lists are out of order in this struct
    uint16_t* appinclusterlist;  //This is to make it possible to pass part of the struct without changes when lists are empty
} AfRegisterReq;

typedef struct __attribute__ ((packed)){
    uint16_t dstaddr;
    uint8_t destendpoint;
    uint8_t srcendpoint;
    uint16_t clusterid;
    uint8_t transid;
    uint8_t options;
    uint8_t radius;
    uint8_t len;
    uint8_t data[240];
} AfDataReqReq;

typedef struct __attribute__ ((packed)){
    uint8_t status;
} AfDataReqResp;

typedef struct __attribute__ ((packed)){
    uint8_t status;
} AfRegisterResp;

typedef struct __attribute__ ((packed)){
    uint8_t status;
    uint8_t endpoint;
    uint8_t transid;
} AfDataConfirmReq;

typedef struct __attribute__ ((packed)){
    uint16_t groupid;
    uint16_t clusterid;
    uint16_t srcaddr;
    uint8_t srcendpoint;
    uint8_t dstendpoint;
    uint8_t wasbroadcast;
    uint8_t linkquality;
    uint8_t securityuse;
    uint32_t timestamp;
    uint8_t transseqnumber;
    uint8_t len;
    uint8_t data[233];
} AfIncomingMsgReq;

typedef struct __attribute__ ((packed)){
    uint8_t type;
} SysResetReq;

typedef struct __attribute__ ((packed)){
    uint8_t transportrev;
    uint8_t product;
    uint8_t majorrel;
    uint8_t minorrel;
    uint8_t maintrel;
    uint32_t revision;
} SysVersionResp;

typedef struct __attribute__ ((packed)){
    uint16_t id;
    uint16_t len;
    uint8_t initlen;
    uint8_t initValue[246];
} SysOsalNvItemInitReq;

typedef struct __attribute__ ((packed)){
    uint8_t status;
} SysOsalNvItemInitResp;

typedef struct __attribute__ ((packed)){
    uint16_t id;
    uint8_t offset;
} SysOsalNvReadReq;

typedef struct __attribute__ ((packed)){
    uint8_t status;
    uint8_t len;
    uint8_t value[1]; // Placeholder. Actual length of this array will be len. This structure will only be casted, not constructed.
} SysOsalNvReadResp;

typedef struct __attribute__ ((packed)){
    uint16_t id;
    uint8_t offset;
    uint8_t len;
    uint8_t value[246];// Because of frame format, it can never be longer
} SysOsalNvWriteReq;

typedef struct __attribute__ ((packed)){
    uint8_t status;
} SysOsalNvWriteResp;

typedef struct __attribute__ ((packed)){
    uint8_t reason;
    uint8_t transportrev;
    uint8_t productid;
    uint8_t majorrel;
    uint8_t minorrel;
    uint8_t hwrev;
} SysResetIndReq;

typedef struct __attribute__ ((packed)){
    uint8_t configid;
} SapiReadConfigurationReq;

typedef struct __attribute__ ((packed)){
    uint8_t status;
    uint8_t configid;
    uint8_t len;
    uint8_t value[247];
} SapiReadConfigurationResp;

typedef struct __attribute__ ((packed)){
    uint8_t configid;
    uint8_t len;
    uint8_t value[246]; // Because of frame format, it can never be longer
} SapiWriteConfigurationReq;

typedef struct __attribute__ ((packed)){
    uint8_t status;
} SapiWriteConfigurationResp;

typedef struct __attribute__ ((packed)){
    uint8_t status;
    uint64_t ieeeaddr;
    uint16_t shortaddr;
    uint8_t devicetype;
    uint8_t devicestate;
    uint8_t numassocdevices;
    uint16_t assocdeviceslist[118]; //More would not fit in a dataframe
} UtilGetDeviceInfoResp;

typedef struct __attribute__ ((packed)){
    uint16_t dstaddr;
    uint16_t nwkaddrofinterest;
} ZdoNodeDescReqReq;

typedef struct __attribute__ ((packed)){
    uint8_t status;
} ZdoNodeDescReqResp;

typedef struct __attribute__ ((packed)){
    uint16_t dstaddr;
    uint16_t nwkaddrofinterest;
    uint8_t endpoint;
} ZdoSimpleDescReqReq;

typedef struct __attribute__ ((packed)){
    uint8_t status;
} ZdoSimpleDescReqResp;

typedef struct __attribute__ ((packed)){
    uint16_t dstaddr;
    uint16_t nwkaddrofinterest;
} ZdoActiveEpReqReq;

typedef struct __attribute__ ((packed)){
    uint8_t status;
} ZdoActiveEpReqResp;

typedef struct __attribute__ ((packed)){
    uint16_t startdelay;
} ZdoStartupFromAppReq;

typedef struct __attribute__ ((packed)){
    uint8_t status;
} ZdoStartupFromAppResp;

typedef struct __attribute__ ((packed)){
    uint16_t srcaddr;
    uint8_t status;
    uint16_t nwkaddr;
    uint8_t logicaltype_cmplxdescavai_userdescavai;
    uint8_t apsflags_freqband;
    uint8_t maccapflags;
    uint16_t manufacturercode;
    uint8_t maxbuffersize;
    uint16_t maxintransfersize;
    uint16_t servermask;
    uint16_t maxouttransfersize;
    uint8_t descriptorcap;
} ZdoNodeDescRspReq;

typedef struct __attribute__ ((packed)){
    uint16_t srcaddr;
    uint8_t status;
    uint16_t nwkaddr;
    uint8_t len;
    uint8_t endpoint;
    uint16_t profileid;
    uint16_t deviceid;
    uint8_t deviceversion;
    uint8_t clusterInfo[238];
} ZdoSimpleDescRspReq;

typedef struct __attribute__ ((packed)){
    uint16_t srcaddr;
    uint8_t status;
    uint16_t nwkaddr;
    uint8_t activeepcount;
    uint8_t activeeplist[244];
} ZdoActiveEpRspReq;

typedef struct __attribute__ ((packed)){
    uint8_t status;
} ZdoStateChangeIndReq;

typedef struct __attribute__ ((packed)){
    uint16_t nwkaddr;
    uint64_t extaddr;
    uint16_t parentaddr;
} ZdoDeviceIndReq;

typedef struct __attribute__ ((packed)){
    uint16_t srcaddr;
    uint16_t nwkaddr;
    uint64_t ieeeaddr;
    uint8_t capabilities;
} ZdoDeviceAnnceIndReq;

typedef struct __attribute__ ((packed)){
    uint16_t srcaddr;
    uint64_t extaddr;
    uint8_t request;
    uint8_t removechildren;
    uint8_t rejoin;
} ZdoLeaveIndReq;