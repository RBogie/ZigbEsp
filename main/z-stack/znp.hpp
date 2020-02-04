#pragma once

#include "serial.hpp"
#include "ZnpFrame.h"

#include <list>
#include <tuple>
#include <future>
#include <mutex>
#include <chrono>
#include <functional>

class ZNP {
public:
    typedef std::function<bool(const ZnpFrame& frame)> FrameWaitConditional;
    typedef std::function<void(const ZnpFrame& frame)> FrameHandler;

    ZNP();
    bool start();
    void setFrameHandler(const FrameHandler &frameHandler);

    std::future<ZnpFrame> request(const ZnpFrame& frame);
    std::future<ZnpFrame> waitFor(Type type, Subsystem subSystem, CommandId commandId);
    std::future<ZnpFrame> waitFor(Type type, Subsystem subSystem, CommandId commandId, FrameWaitConditional);

    template<class T, class Rep, class Period> T request(const ZnpFrame& frame, const std::chrono::duration<Rep, Period>& timeout) {
        auto future = request(frame);
        if(!future.valid()) {
            throw std::runtime_error("Typed request done on call without result");
        }
        if (future.wait_for(std::chrono::milliseconds(TIMEOUT_REQUEST_SREQ)) != std::future_status::ready) {
            throw std::runtime_error("Timeout while retrieving data");
        }
        auto result = future.get();
        if(result.dataLen > sizeof(T)) {
            throw std::runtime_error("Could not fit result in returntype");
        }
        T t;
        memcpy(&t, result.getData(), result.dataLen);
        return std::move(t);
    }
private:
    void onFrameRead(const ZnpFrame& frame);

    void skipBootloader();
    SerialPort port;
    FrameHandler frameHandler;

private:

    std::list<std::tuple<std::promise<ZnpFrame>, Type, Subsystem, CommandId, FrameWaitConditional>> frameWaitQueue;
    std::mutex frameWaitQueueLock;


};