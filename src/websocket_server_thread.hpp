#pragma once
#ifndef WEBSOCKET_SERVER_THREAD_H
#define WEBSOCKET_SERVER_THREAD_H

#include "server_thread_interface.hpp"
#include "websocket_server_interface.hpp"
#include "server_thread.hpp"
#include "server.hpp"

// standard
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <nlohmann/json.hpp>
#include <memory>
#include <thread>

class WebsocketFrameRaw
{
public:
    WebsocketFrameRaw(char *buffer);

    ~WebsocketFrameRaw()
    {
        delete[] this->payload_;
    }

protected:
    bool fin_;
    bool rsv1_;
    bool rsv2_;
    bool rsv3_;
    char opcode_;
    bool mask_;
    unsigned long long payload_length_;
    unsigned int masking_key_;
    char *payload_;

private:
    char *buffer_;
};

WebsocketFrameRaw::WebsocketFrameRaw(char *buffer) : buffer_(buffer)
{
    this->fin_ = (buffer[0] & 0b10000000) >> 7;
    this->rsv1_ = (buffer[0] & 0b01000000) >> 6;
    this->rsv2_ = (buffer[0] & 0b00100000) >> 5;
    this->rsv3_ = (buffer[0] & 0b00010000) >> 4;
    this->opcode_ = buffer[0] & 0b00001111;
    this->mask_ = (buffer[1] & 0b10000000) >> 7;
    this->payload_length_ = buffer[1] & 0b01111111;

    unsigned int offset = 2;

    if (this->payload_length_ == 126)
    {
        this->payload_length_ = (buffer[2] << 8) + buffer[3];
        offset += 2;
    }
    else if (this->payload_length_ == 127)
    {
        this->payload_length_ = (buffer[2] << 56) + (buffer[3] << 48) + (buffer[4] << 40) + (buffer[5] << 32) + (buffer[6] << 24) + (buffer[7] << 16) + (buffer[8] << 8) + buffer[9];
        offset += 8;
    }

    if (this->mask_)
    {
        this->masking_key_ = (buffer[offset] << 24) + (buffer[offset + 1] << 16) + (buffer[offset + 2] << 8) + buffer[offset + 3];
        offset += 4;
    }

    this->payload_ = new char[this->payload_length_];
    memcpy(this->payload_, buffer + offset, this->payload_length_);
}

enum class WebsocketOpcode
{
    CONTINUATION = 0x0,
    TEXT = 0x1,
    BINARY = 0x2,
    CLOSE = 0x8,
    PING = 0x9,
    PONG = 0xA
};

class WebsocketFrame : public WebsocketFrameRaw
{
public:
    WebsocketFrame(char *buffer) : WebsocketFrameRaw(buffer)
    {
        if (this->mask_)
            for (int i = 0; i < this->payload_length_; i++)
                this->payload_[i] = this->payload_[i] ^ ((char *)&this->masking_key_)[i % 4];
    }

    WebsocketOpcode opcode()
    {
        return static_cast<WebsocketOpcode>(this->opcode_);
    }

    std::string toString()
    {
        std::string result = "";
        result += "fin: " + std::to_string(this->fin_) + "\n";
        result += "rsv1: " + std::to_string(this->rsv1_) + "\n";
        result += "rsv2: " + std::to_string(this->rsv2_) + "\n";
        result += "rsv3: " + std::to_string(this->rsv3_) + "\n";
        result += "opcode: " + std::to_string(this->opcode_) + "\n";
        result += "mask: " + std::to_string(this->mask_) + "\n";
        result += "payload_length: " + std::to_string(this->payload_length_) + "\n";
        result += "masking_key: " + std::to_string(this->masking_key_) + "\n";
        result += "payload: " + std::string(this->payload_) + "\n";
        return result;
    }
};

class WebsocketServerThread : public BaseServerThread
{
public:
    WebsocketServerThread(std::unique_ptr<ClientConnectionMetadata> connectionMetadata, std::weak_ptr<BaseWebsocketServer> server) : connectionMetadata_(std::move(connectionMetadata)), server_(server)
    {
        std::thread thread(&WebsocketServerThread::start_handling, this);
        thread.detach();
    }
    void start_handling() override;
    bool yeet() override { return yeet_flag; }

    ~WebsocketServerThread() override
    {
        std::cout << "WebsocketServerThread destructor called" << std::endl;
    }

private:
    bool yeet_flag = false;
    std::unique_ptr<ClientConnectionMetadata> connectionMetadata_;
    std::weak_ptr<BaseWebsocketServer> server_;
};

#endif // !WEBSOCKET_SERVER_THREAD_H

void WebsocketServerThread::start_handling()
{
    std::cout << "Upgraded to websocket" << std::endl;
    char buffer[1024] = {0};

    while (true)
    {
        memset(buffer, 0, sizeof(buffer));

        int bytes_read = read(this->connectionMetadata_->get(), buffer, sizeof(buffer));
        if (bytes_read <= 0)
        {
            this->yeet_flag = true;
            break;
        }

        WebsocketFrame frame(buffer);

        std::cout << frame.toString() << std::endl;

        if (frame.opcode() == WebsocketOpcode::CLOSE)
        {
            this->yeet_flag = true;
            break;
        }
    }
}