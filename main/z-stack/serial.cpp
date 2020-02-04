#include "serial.hpp"

#include "driver/uart.h"
#include "esp_pthread.h"

#include <pthread.h>
#include <iostream>
#include <utility>

void* SerialPort::readPortBootstrap(void* parameter) {
    ((SerialPort*)parameter)->readPort();
    return nullptr;
}

SerialPort::SerialPort(FrameReadCallback callback): frameReadCallback(std::move(callback)) {}

bool SerialPort::open() {
    uart_config_t uart_config = {
            .baud_rate = 115200,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    // Configure UART parameters
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_2, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_2, 17, 16, 18, 19));
    QueueHandle_t uart_queue;
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_2, 2048, 0, 10, &uart_queue, 0));

    this->initialized = true;

    esp_pthread_cfg_t cfg = esp_pthread_get_default_config();
    cfg.thread_name = "serialRead";
    esp_pthread_set_cfg(&cfg);
    int status = pthread_create( &readThread, nullptr, readPortBootstrap, (void*) this);

    return true;
}

int SerialPort::write(const uint8_t* buff, size_t buffLen) {
    if(!this->initialized)
        return -1;

    return uart_write_bytes(UART_NUM_2, (const char*)buff, buffLen);
}
void SerialPort::write(const ZnpFrame& frame) {
    if(!this->initialized)
        return;

    std::cout << "Writing frame: type=" << (int)frame.type<<", system="<<(int)frame.subsystem << ", commandId=" << (int)frame.commandId << ", payloadLength=" << (int)frame.dataLen << std::endl;

    uint8_t buffer[255];
    uint8_t size = frame.toBuffer(buffer, 255);
    uart_write_bytes(UART_NUM_2, (const char*)buffer, size);
}

void SerialPort::readPort() {
    while(true) {
        uint8_t c;
        if(!uart_read_bytes(UART_NUM_2, &c, 1, portMAX_DELAY)) {
            continue;
        }
        if(!foundFrameStart) {
            if(c == SOF) {
                foundFrameStart = true;
            } else {
                continue;
            }
        }
        frameBuffer[frameBufferPos] = c;

        if(frameBufferPos >= 4) {
            //Since we have a minimum framelength, we can start trying to decode the frame.
            uint8_t frameLen = frameBuffer[1];
            if(frameBufferPos == (frameLen + 4)) {
                auto frame = ZnpFrame::fromBuffer(frameBuffer, frameBufferPos);
                if(frame) {
                    frameReadCallback(*frame);
                }
                frameBufferPos = 0;
                foundFrameStart = false;
                continue;
            }
        }
        frameBufferPos++;
    }
}

void SerialPort::close() {
    uart_driver_delete(UART_NUM_2);
}

SerialPort::~SerialPort() {
    if(this->initialized)
        this->close();
}