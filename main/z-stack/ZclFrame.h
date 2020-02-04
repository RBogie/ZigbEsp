#pragma once

#include "zcl.h"
#include "clusters.h"
#include <memory>
class ZclFrame {
public:
    ZclFrame(
            ZclFrameType type,
            Direction direction,
            bool disableDefaultResponse,
            uint16_t manufacturerCode,
            uint8_t transactionSequence,
            ClusterId clusterId,
            ZclCommandId commandId,
            const uint8_t* const payload,
            uint8_t payloadLen);

    uint8_t toBuffer(uint8_t* buff, size_t buffLen) const;
    static ZclFrame fromBuffer(ClusterId clusterId, const uint8_t* const buff, size_t buffLen);

    ZclFrameType getType() const;
    Direction getDirection() const;
    bool isDisableDefaultResponse() const;
    uint16_t getManufacturerCode() const;
    uint8_t getTransactionSequence() const;
    ClusterId getClusterId() const;
    ZclCommandId getCommandId() const;
    const uint8_t* getPayload() const;
    uint8_t getPayloadLength() const;

private:
    ZclFrameType type;
    Direction direction;
    bool disableDefaultResponse;
    uint16_t manufacturerCode;
    uint8_t transactionSequence;
    ClusterId clusterId;
    ZclCommandId commandId;

    uint8_t payload[247];
    uint8_t payloadLength;
};