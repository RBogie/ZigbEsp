#pragma once

#include "ZnpFrame.h"
#include <cstddef>
#include <functional>

#include <pthread.h>

typedef std::function<void(const ZnpFrame& frame)> FrameReadCallback;

class SerialPort {
public:
    SerialPort(FrameReadCallback callback);
    bool open();
    int write(const uint8_t* buff, size_t buffLen);
    void write(const ZnpFrame&frame);
    void close();
    ~SerialPort();
private:
    bool initialized = false;
    FrameReadCallback frameReadCallback;
    pthread_t readThread;

    //Needed for reading full frames
    bool foundFrameStart;
    uint8_t frameBuffer[255];
    uint8_t frameBufferPos = 0;

    static void* readPortBootstrap(void* parameter);
    void readPort();
};