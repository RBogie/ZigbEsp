#include "znp.hpp"
#include "serial.hpp"

#include <unistd.h>
#include <iostream>
#include <iomanip>

ZNP::ZNP() : port(std::bind(&ZNP::onFrameRead, this, std::placeholders::_1)) {

}

void ZNP::onFrameRead(const ZnpFrame &frame) {
    std::cout << "Read frame: type=" << (int)frame.type<<", system="<<(int)frame.subsystem << ", commandId=" << (int)frame.commandId << ", payloadLength=" << (int)frame.dataLen << std::endl;

    //TODO: handle errors
    std::lock_guard<std::mutex> guard(frameWaitQueueLock);
    auto it = frameWaitQueue.begin();
    while(it != frameWaitQueue.end()) {
        if(std::get<1>((*it)) == frame.type && std::get<2>((*it)) == frame.subsystem && std::get<3>((*it)) == frame.commandId) {
            if(std::get<4>(*it) && !std::get<4>(*it)(frame)) {
                it++;
                continue; // Check the conditional
            }
            std::get<0>((*it)).set_value(frame);
            frameWaitQueue.erase(it);
            break;
        }
        it++;
    }

    if(frameHandler)
        frameHandler(frame);
}

bool ZNP::start() {
    if(!port.open())
        return false;

    this->skipBootloader();

    return true;
}

void ZNP::skipBootloader() {
    uint8_t magicByte = 0xef;
    port.write(&magicByte, 1);
    sleep(1); //Give ZNP 1 second to boot
}

std::future<ZnpFrame> ZNP::request(const ZnpFrame& frame) {
    if(frame.type == Type::SREQ) {
        auto future = this->waitFor(Type::SRSP, frame.subsystem, frame.commandId);
        port.write(frame);
        return future;
    } else if(frame.type == Type::AREQ && frame.subsystem == Subsystem::SYS && frame.commandId == CommandId::SYS_RESET_REQ) {
        auto future = this->waitFor(Type::AREQ, Subsystem::SYS, CommandId::SYS_RESET_IND);
        port.write(frame);
        return future;
    } else {
        return std::future<ZnpFrame>();
    }
}

std::future<ZnpFrame> ZNP::waitFor(Type type, Subsystem subSystem, CommandId commandId) {
    std::lock_guard<std::mutex> guard(frameWaitQueueLock);

    std::promise<ZnpFrame> promise = std::promise<ZnpFrame>();
    auto future = promise.get_future();
    frameWaitQueue.emplace_front(std::move(promise), type, subSystem, commandId, FrameWaitConditional());
    return future;
}

std::future<ZnpFrame> ZNP::waitFor(Type type, Subsystem subSystem, CommandId commandId, FrameWaitConditional conditional) {
    std::lock_guard<std::mutex> guard(frameWaitQueueLock);

    std::promise<ZnpFrame> promise = std::promise<ZnpFrame>();
    auto future = promise.get_future();
    frameWaitQueue.emplace_front(std::move(promise), type, subSystem, commandId, conditional);
    return future;
}

void ZNP::setFrameHandler(const ZNP::FrameHandler &frameHandler) {
    this->frameHandler = frameHandler;
}
