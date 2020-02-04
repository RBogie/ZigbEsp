#include "zcl.h"

#include <memory>
#include <cstring>

ZclData::ZclData(ZclDataType type, std::unique_ptr<char[]> str): type(type), str(std::move(str)) {}
ZclData::ZclData(ZclDataType type, uint8_t val): type(type), uint8Val(val) {}

ZclDataType ZclData::getType() const {
    return type;
}

const char* ZclData::getStr() const {
    return str.get();
}

uint8_t ZclData::getUint8Val() const {
    return uint8Val;
}

uint16_t ZclData::getUint16Val() const {
    return uint16Val;
}

uint32_t ZclData::getUint32Val() const {
    return uint32Val;
}

uint64_t ZclData::getUint64Val() const {
    return uint64Val;
}

int8_t ZclData::getInt8Val() const {
    return int8Val;
}

int16_t ZclData::getInt16Val() const {
    return int16Val;
}

int32_t ZclData::getInt32Val() const {
    return int32Val;
}

int64_t ZclData::getInt64Val() const {
    return int64Val;
}

ZclData parseData(ZclDataType type, const uint8_t* const buff, uint8_t buffLen, uint8_t* startIndex) {
    switch(type) {
        case ZclDataType::charStr: {
            uint8_t strLen = buff[(*startIndex)++];
            auto str = std::make_unique<char[]>(strLen + 1);
            memcpy(str.get(), &(buff[*startIndex]), strLen);
            str.get()[strLen] = 0;
            *startIndex += strLen;
            return ZclData(type, std::move(str));
        }
        case ZclDataType::data8:
        case ZclDataType::int8:
        case ZclDataType::uint8:
        case ZclDataType::enum8: {
            uint8_t value = buff[(*startIndex)++];
            return ZclData(type, value);
        }
        default:
            throw std::runtime_error("Unknown datatype during parsing");
    }
}

ZclData parseReadRspEntry(const uint8_t* const buff, uint8_t buffLen, uint8_t* startIndex, uint16_t* attr) {
    if(buffLen - *startIndex < 3)
        throw std::runtime_error("Cannot read partial readRsp entry");

    *attr = *((uint16_t*)&(buff[*startIndex]));
    *startIndex += 2;
    uint8_t status = buff[(*startIndex)++];
    if(status != 0) {
        throw std::runtime_error("Attribute return status != 0");
    }

    auto dataType = ZclDataType(buff[(*startIndex)++]);
    return parseData(dataType, buff, buffLen, startIndex);
}