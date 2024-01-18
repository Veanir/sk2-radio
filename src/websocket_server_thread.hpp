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
    WebsocketFrameRaw(char *buffer, int buffer_size);

    WebsocketFrameRaw(){};

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
    char masking_key_[4];
    char *payload_;

    int length_bytes_count_;

private:
    char *buffer_;
};

WebsocketFrameRaw::WebsocketFrameRaw(char *buffer, int buffer_size) : buffer_(buffer)
{
    if (buffer_size < 2)
        throw std::runtime_error("Buffer size too small");
    this->fin_ = (buffer[0] & 0b10000000) >> 7;
    this->rsv1_ = (buffer[0] & 0b01000000) >> 6;
    this->rsv2_ = (buffer[0] & 0b00100000) >> 5;
    this->rsv3_ = (buffer[0] & 0b00010000) >> 4;
    this->opcode_ = buffer[0] & 0b00001111;
    this->mask_ = (buffer[1] & 0b10000000) >> 7;
    this->payload_length_ = buffer[1] & 0b01111111;
    this->length_bytes_count_ = 1;

    unsigned int offset = 2;

    if (this->payload_length_ == 126)
    {
        if (buffer_size < 4)
            throw std::runtime_error("Buffer size too small");
        this->payload_length_ = (buffer[2] << 8) + buffer[3];
        this->length_bytes_count_ = 2;
        offset += 2;
    }
    else if (this->payload_length_ == 127)
    {
        if (buffer_size < 10)
            throw std::runtime_error("Buffer size too small");
        this->payload_length_ = (buffer[2] << 56) + (buffer[3] << 48) + (buffer[4] << 40) + (buffer[5] << 32) + (buffer[6] << 24) + (buffer[7] << 16) + (buffer[8] << 8) + buffer[9];
        this->length_bytes_count_ = 8;
        offset += 8;
    }

    if (this->mask_)
    {
        if (buffer_size < offset + 4)
            throw std::runtime_error("Buffer size too small");
        memcpy(this->masking_key_, buffer + offset, 4);
        offset += 4;
    }

    if (buffer_size < offset + this->payload_length_)
        throw std::runtime_error("Buffer size too small");
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
    WebsocketFrame(char *buffer, int buffer_size) : WebsocketFrameRaw(buffer, buffer_size)
    {
        if (this->mask_)
            for (int i = 0; i < this->payload_length_; i++)
                this->payload_[i] = this->payload_[i] ^ ((char *)&this->masking_key_)[i % 4];
    }

    WebsocketFrame(WebsocketOpcode opcode, std::string payload, bool fin)
    {
        this->fin_ = fin;
        this->opcode_ = (char)opcode;
        this->payload_length_ = payload.length();
        if (this->payload_length_ < 126)
            this->length_bytes_count_ = 1;
        else if (this->payload_length_ < 65536)
            this->length_bytes_count_ = 2;
        else
            this->length_bytes_count_ = 8;
        this->payload_ = new char[this->payload_length_];
        memcpy(this->payload_, payload.c_str(), this->payload_length_);
    }

    char *payload()
    {
        return this->payload_;
    }

    unsigned long long payload_length()
    {
        return this->payload_length_;
    }

    WebsocketOpcode opcode()
    {
        return (WebsocketOpcode)this->opcode_;
    }

    bool finished()
    {
        return this->fin_;
    }

    std::unique_ptr<char[]> toBuffer(unsigned long long &buffer_length)
    {
        int buffer_size = 2;

        if (this->length_bytes_count_ == 2)
            buffer_size += 2;
        else if (this->length_bytes_count_ == 8)
            buffer_size += 8;
        std::unique_ptr<char[]> buffer = std::make_unique<char[]>(buffer_size + this->payload_length_);

        buffer[0] = (this->fin_ << 7) + this->opcode_;
        buffer[1] = (this->payload_length_ & 0b01111111); // No mask

        if (this->length_bytes_count_ == 2)
        {
            buffer[1] = 126;
            buffer[2] = (this->payload_length_ >> 8) & 0xFF;
            buffer[3] = this->payload_length_ & 0xFF;
        }
        else if (this->length_bytes_count_ == 8)
        {
            buffer[1] = 127;
            buffer[2] = (this->payload_length_ >> 56) & 0xFF;
            buffer[3] = (this->payload_length_ >> 48) & 0xFF;
            buffer[4] = (this->payload_length_ >> 40) & 0xFF;
            buffer[5] = (this->payload_length_ >> 32) & 0xFF;
            buffer[6] = (this->payload_length_ >> 24) & 0xFF;
            buffer[7] = (this->payload_length_ >> 16) & 0xFF;
            buffer[8] = (this->payload_length_ >> 8) & 0xFF;
            buffer[9] = this->payload_length_ & 0xFF;
        }

        memcpy(buffer.get() + buffer_size, this->payload_, this->payload_length_);

        buffer_length = buffer_size + this->payload_length_;

        return buffer;
    }
};

class WebsocketServerThread : public BaseServerThread, public IAudioListener
{
public:
    WebsocketServerThread(std::unique_ptr<ClientConnectionMetadata> connectionMetadata, std::weak_ptr<BaseWebsocketServer> server) : connectionMetadata_(std::move(connectionMetadata)), server_(server)
    {
        std::thread thread(&WebsocketServerThread::start_handling, this);
        thread.detach();
    }
    void start_handling() override;
    bool yeet() override { return yeet_flag; }

    void on_audio_block(std::shared_ptr<AudioBlock> block) override;

    void on_queue_change(nlohmann::json queue) override;

    ~WebsocketServerThread() override
    {
        std::cout << "WebsocketServerThread destructor called" << std::endl;
    }

private:
    bool yeet_flag = false;
    std::unique_ptr<ClientConnectionMetadata> connectionMetadata_;
    std::weak_ptr<BaseWebsocketServer> server_;
};

void WebsocketServerThread::start_handling()
{
    std::cout << "Upgraded to websocket" << std::endl;
    char buffer[1024 * 128] = {0};

    while (this->yeet_flag == false)
    {
        memset(buffer, 0, sizeof(buffer));

        int bytes_read = read(this->connectionMetadata_->get(), buffer, sizeof(buffer));
        if (bytes_read <= 0)
        {
            this->yeet_flag = true;
            break;
        }

        try
        {
            WebsocketFrame frame(buffer, bytes_read);

            if (frame.opcode() == WebsocketOpcode::CLOSE)
            {
                this->yeet_flag = true;
                break;
            }

            if (frame.opcode() == WebsocketOpcode::PING)
            {
                unsigned long long buffer_size;
                std::unique_ptr<char[]> buffer = WebsocketFrame(WebsocketOpcode::PONG, "PONG", true).toBuffer(buffer_size);
                write(this->connectionMetadata_->get(), buffer.get(), buffer_size);
            }

            if (frame.opcode() == WebsocketOpcode::CLOSE)
            {
                this->yeet_flag = true;
                break;
            }

            if (frame.opcode() == WebsocketOpcode::BINARY && frame.finished())
            {
                std::cout << "Received binary frame" << std::endl;
                continue;
            }

            // std::cout << "Received message: " << std::string(frame.payload(), frame.payload_length()) << std::endl;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Failed to parse Websocket frame: " << e.what() << '\n';
        }
    }

    this->yeet_flag = true;
}

void WebsocketServerThread::on_audio_block(std::shared_ptr<AudioBlock> block)
{
    nlohmann::json json;
    json["audio_block"]["duration"] = block->duration;
    json["audio_block"]["rate"] = block->sampling_rate;
    json["audio_block"]["data"] = block->base64();

    unsigned long long buffer_size;
    std::unique_ptr<char[]> buffer = WebsocketFrame(WebsocketOpcode::TEXT, json.dump(), true).toBuffer(buffer_size);
    ssize_t result = write(this->connectionMetadata_->get(), buffer.get(), buffer_size);
    if (result == -1)
    {
        if (errno == EPIPE)
        {
            std::cerr << "Broken pipe encountered" << std::endl;
        }
        else
        {
            std::cerr << "Failed to write to socket: " << strerror(errno) << std::endl;
        }
        this->yeet_flag = true;
    }
}

void WebsocketServerThread::on_queue_change(nlohmann::json queue)
{
    unsigned long long buffer_size;
    std::unique_ptr<char[]> buffer = WebsocketFrame(WebsocketOpcode::TEXT, queue.dump(), true).toBuffer(buffer_size);
    write(this->connectionMetadata_->get(), buffer.get(), buffer_size);
}

#endif // !WEBSOCKET_SERVER_THREAD_H