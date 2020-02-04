#pragma once
#include "constants.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>

class ZnpFrame {
public:
    ZnpFrame(Type type, Subsystem subsystem, CommandId commandId, const uint8_t* data, uint8_t dataLen);
    uint8_t toBuffer(uint8_t* buff, size_t buffLen) const;
    static std::unique_ptr<ZnpFrame> fromBuffer(uint8_t* buff, size_t buffLen);

    const Type type;
    const Subsystem subsystem;
    const CommandId commandId;
    const uint8_t dataLen;

    const uint8_t* const getData() const;

    template<class T> const T* const getData() const {
        return (T*) data;
    }
private:
    uint8_t data[250];

    static uint8_t calculateChecksum(const uint8_t* data, size_t len);
};
