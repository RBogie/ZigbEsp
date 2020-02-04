#pragma once

#include <cstdint>
#include <stdexcept>
#include <memory>
#include "clusters.h"

enum class ZclFrameType : uint8_t {
    GLOBAL = 0,
    SPECIFIC = 1,
};

enum class Direction : uint8_t {
    CLIENT_TO_SERVER = 0,
    SERVER_TO_CLIENT = 1,
};

enum class ZclCommandId : uint8_t {
    GLOBAL_READ = 0,
    GLOBAL_READ_RSP = 1,

    GEN_ONOFF_TOGGLE = 2,
};

enum class PowerSource : uint8_t {
    UNKNOWN = 0,
    MAINS_SINGLE_PHASE = 1,
    MAINS_3_PHASE = 2,
    BATTERY = 3,
    DC_SOURCE = 4,
    EMERGENCY_CONSTANT = 5,
    EMERGENCY_TRANSFER = 6
};

////////////////////
/// ZCL Payloads ///
////////////////////

typedef struct __attribute__ ((packed)){
    uint16_t attrId;
} ZclGlobalReadReq_Rep;

///////////////////////
/// Utility methods ///
///////////////////////

enum class ZclDataType : uint8_t {
    noData = 0,
    data8 = 8,
    data16 = 9,
    data24 = 10,
    data32 = 11,
    data40 = 12,
    data48 = 13,
    data56 = 14,
    data64 = 15,
    boolean = 16,
    bitmap8 = 24,
    bitmap16 = 25,
    bitmap24 = 26,
    bitmap32 = 27,
    bitmap40 = 28,
    bitmap48 = 29,
    bitmap56 = 30,
    bitmap64 = 31,
    uint8 = 32,
    uint16 = 33,
    uint24 = 34,
    uint32 = 35,
    uint40 = 36,
    uint48 = 37,
    uint56 = 38,
    uint64 = 39,
    int8 = 40,
    int16 = 41,
    int24 = 42,
    int32 = 43,
    enum8 = 48,
    enum16 = 49,
    singlePrec = 57,
    doublePrec = 58,
    octetStr = 65,
    charStr = 66,
    longOctetStr = 67,
    longCharStr = 68,
    array = 72,
    structType = 76,
    set = 80,
    bag = 81,
    tod = 224,
    date = 225,
    utc = 226,
    clusterId = 232,
    attrId = 233,
    bacOid = 234,
    ieeeAddr = 240,
    secKey = 241,
    unknown = 255,
};

class ZclData {
public:
    ZclData(ZclDataType type, std::unique_ptr<char[]> str);
    ZclData(ZclDataType type, uint8_t val);

    ZclDataType getType() const;

    const char* getStr() const;

    uint8_t getUint8Val() const;

    uint16_t getUint16Val() const;

    uint32_t getUint32Val() const;

    uint64_t getUint64Val() const;

    int8_t getInt8Val() const;

    int16_t getInt16Val() const;

    int32_t getInt32Val() const;

    int64_t getInt64Val() const;

private:
    ZclDataType type;
    std::unique_ptr<char[]> str;
    union {
        uint8_t uint8Val;
        uint16_t uint16Val;
        uint32_t uint32Val;
        uint64_t uint64Val;
        int8_t int8Val;
        int16_t int16Val;
        int32_t int32Val;
        int64_t int64Val;
    };
};

ZclData parseReadRspEntry(const uint8_t* const buff, uint8_t buffLen, uint8_t* startIndex, uint16_t* attr);