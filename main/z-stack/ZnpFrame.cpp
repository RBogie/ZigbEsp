#include "ZnpFrame.h"

#include <cstring>

#include <iostream>
#include <iomanip>

ZnpFrame::ZnpFrame(
        Type type,
        Subsystem subsystem,
        CommandId commandId,
        const uint8_t *data,
        uint8_t dataLen)
            : type(type), subsystem(subsystem), commandId(commandId), dataLen(dataLen) {
    if(dataLen > 250) {
        throw std::runtime_error("frame datalength too big");
    }
    memcpy(this->data, data, dataLen);
}

uint8_t ZnpFrame::toBuffer(uint8_t *buff, size_t buffLen) const{
    //First check if buffer is sufficiently long
    if(buffLen < (5 + dataLen)) {
        std::cerr << "Supplied frame buffer too small" << std::endl;
        return 0;
    }

    uint8_t cmd0 = ((((uint8_t)this->type) << 5) & 0xE0) | (((uint8_t)this->subsystem) & 0x1F);

    buff[0] = SOF;
    buff[1] = (uint8_t) dataLen;
    buff[2] = cmd0;
    buff[3] = (uint8_t)commandId;
    memcpy((buff+4), data, dataLen);
    buff[4 + dataLen] = calculateChecksum(buff+1, dataLen+3);

    return 5+dataLen;
}

std::unique_ptr<ZnpFrame> ZnpFrame::fromBuffer(uint8_t *buff, size_t buffLen) {
    if(buffLen < 4) {
        std::cerr << "Source frame buffer too small" << std::endl;
        return std::unique_ptr<ZnpFrame>();
    }
    if(buff[0] != SOF) {
        std::cerr << "Incorrect framestart" << std::endl;
        return std::unique_ptr<ZnpFrame>();
    }

    uint8_t len = buff[1];

    if(len > 250) {
        throw std::runtime_error("frame data length too big");
    }

    if(buffLen < len+4) {
        std::cerr << "Incomplete frame supplied" << std::endl;
        return std::unique_ptr<ZnpFrame>();
    }



    Subsystem subSys = Subsystem(buff[2] & 0x1F);
    Type type= Type((buff[2] & 0xE0) >> 5);

    uint8_t checksum = calculateChecksum(&(buff[1]), len+3);
    if(checksum != buff[len+4]) {
        std::cout << std::setfill('0') << std::setw(2) << std::hex ;
        std::cerr << "ZnpFrame checksum mismatch. Calculated 0x" << (int)checksum << ", received 0x" << (int)buff[len+4] << std::endl;
        std::cout << std::setfill('0') << std::setw(0) << std::dec;
        return std::unique_ptr<ZnpFrame>();
    }

    return std::make_unique<ZnpFrame>(type, subSys, CommandId(buff[3]), &buff[4], len);
}

uint8_t ZnpFrame::calculateChecksum(const uint8_t *data, size_t len) {
    uint8_t checksum = 0;

    for(size_t i = 0; i < len; i++) {
        checksum ^= data[i];
    }

    return checksum;
}

const uint8_t *const ZnpFrame::getData() const {
    return data;
}