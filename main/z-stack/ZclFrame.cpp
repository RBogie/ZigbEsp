//
// Created by Rob Bogie on 01/02/2020.
//

#include "ZclFrame.h"
#include <iostream>
#include <cstring>

ZclFrame::ZclFrame(ZclFrameType type, Direction direction, bool disableDefaultResponse, uint16_t manufacturerCode,
                   uint8_t transactionSequence, ClusterId clusterId, ZclCommandId commandId, const uint8_t* const payload,
                   uint8_t payloadLen):
                   type(type),
                   direction(direction),
                   disableDefaultResponse(disableDefaultResponse),
                   manufacturerCode(manufacturerCode),
                   transactionSequence(transactionSequence),
                   clusterId(clusterId),
                   commandId(commandId),
                   payloadLength(payloadLen){
    memcpy(this->payload, payload, payloadLen);
}

uint8_t ZclFrame::toBuffer(uint8_t *buff, size_t buffLen) const {
    if(buffLen < (3 + ((manufacturerCode != 0) ? 2 : 0) + payloadLength)) {
        std::cerr << "Supplied frame buffer too small" << std::endl;
        return 0;
    }

    //Write header
    uint8_t frameControl = ((uint8_t)type & 0x03);
    frameControl |= ((((manufacturerCode != 0) ? 1 : 0) << 2) & 0x04);
    frameControl |= (((uint8_t)direction << 3) & 0x08);
    frameControl |= (((disableDefaultResponse ? 1 : 0) << 4) & 0x10);

    uint8_t index = 0;
    buff[index++] = frameControl;
    if(manufacturerCode != 0) {
        *((uint16_t*)&(buff[index])) = manufacturerCode;
        index+=2;
    }

    buff[index++] = transactionSequence;
    buff[index++] = (uint8_t)commandId;
    memcpy(&(buff[index]), payload, payloadLength);
    index += payloadLength;
    return index;
}

ZclFrame ZclFrame::fromBuffer(ClusterId clusterId, const uint8_t *const buff, size_t buffLen) {
    uint8_t index = 0;
    uint8_t frameControl = buff[index++];
    auto type = ZclFrameType(frameControl & 0x03);
    bool manufacturerSpecific = ((frameControl >> 2) & 0x01) == 0x01;
    auto direction = Direction((frameControl >> 3) & 0x01);
    bool disableDefaultResponse = ((frameControl >> 4) & 0x01) == 0x01;

    uint16_t manufacturerCode = 0;
    if(manufacturerSpecific) {
        manufacturerCode = *((uint16_t*)&(buff[index]));
        index+=2;
    }

    uint8_t transactionSequence = buff[index++];
    auto commandId = ZclCommandId(buff[index++]);

    return ZclFrame(type, direction, disableDefaultResponse, manufacturerCode, transactionSequence, clusterId, commandId, &(buff[index]), buffLen - index);
}

ZclFrameType ZclFrame::getType() const {
    return type;
}

Direction ZclFrame::getDirection() const {
    return direction;
}

bool ZclFrame::isDisableDefaultResponse() const {
    return disableDefaultResponse;
}

uint16_t ZclFrame::getManufacturerCode() const {
    return manufacturerCode;
}

uint8_t ZclFrame::getTransactionSequence() const {
    return transactionSequence;
}

ClusterId ZclFrame::getClusterId() const {
    return clusterId;
}

ZclCommandId ZclFrame::getCommandId() const {
    return commandId;
}

const uint8_t* ZclFrame::getPayload() const {
    return payload;
}

uint8_t ZclFrame::getPayloadLength() const {
    return payloadLength;
}