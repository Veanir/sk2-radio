#pragma once
#ifndef WEBSOCKET_SERVER_THREAD_H
#define WEBSOCKET_SERVER_THREAD_H

#include "audio/audio_file.h"
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

enum class WebsocketOpcode
{
    CONTINUATION = 0x0,
    TEXT = 0x1,
    BINARY = 0x2,
    CLOSE = 0x8,
    PING = 0x9,
    PONG = 0xA
};

enum class WebsocketFrameLengthStage
{
    LENGTH_7_BIT,
    LENGTH_16_BIT,
    LENGTH_64_BIT
};

enum class WebsocketFrameProcessingState
{
    PROCESSING_START,
    OPCODE_PROCESSED,
    MASK_FLAG_PROCESSED,
    LENGTH_PROCESSED,
    MASKING_KEY_PROCESSED,
    FINISHED_PROCESSING
};

class WSStage
{
public:
    virtual WebsocketFrameProcessingState push_data(char byte) = 0;
    virtual void update_state(WebsocketFrameProcessingState state) = 0;
};

class WSFinOpcode : public WSStage
{
public:
    WSFinOpcode(){};

    WebsocketFrameProcessingState push_data(char byte) override
    {
        switch (this->state_)
        {
        case WebsocketFrameProcessingState::PROCESSING_START:
            this->byte_ = byte;
            this->state_ = WebsocketFrameProcessingState::OPCODE_PROCESSED;
            break;
        default:
            break;
        }

        return this->state_;
    }

    void update_state(WebsocketFrameProcessingState state) override
    {
        this->state_ = state;
    }

    bool fin()
    {
        return (this->byte_ >> 7) & 0x1;
    }

    WebsocketOpcode opcode()
    {
        return (WebsocketOpcode)(this->byte_ & 0xF);
    }

    char rsv()
    {
        return (this->byte_ >> 4) & 0x7;
    }

private:
    WebsocketFrameProcessingState state_ = WebsocketFrameProcessingState::PROCESSING_START;
    char byte_;
};

class WSMaskLength : public WSStage
{
public:
    WSMaskLength(){};

    WebsocketFrameProcessingState push_data(char byte) override
    {
        switch (this->state_)
        {
        case WebsocketFrameProcessingState::OPCODE_PROCESSED:
            this->masked_ = (byte >> 7) & 0x1;
            this->length_ = byte & 0x7F;
            this->state_ = WebsocketFrameProcessingState::MASK_FLAG_PROCESSED;

            if (this->length_ < 126)
                this->state_ = WebsocketFrameProcessingState::LENGTH_PROCESSED;
            else if (this->length_ == 126)
            {
                this->length_stage_ = WebsocketFrameLengthStage::LENGTH_16_BIT;
                this->length_buffer_.reserve(2);
            }
            else if (this->length_ == 127)
            {
                this->length_stage_ = WebsocketFrameLengthStage::LENGTH_64_BIT;
                this->length_buffer_.reserve(8);
            }
            break;
        case WebsocketFrameProcessingState::MASK_FLAG_PROCESSED:
            if (this->state_ == WebsocketFrameProcessingState::LENGTH_PROCESSED)
                break;

            this->length_buffer_.push_back(byte);
            if (this->length_buffer_.size() == 2 && this->length_stage_ == WebsocketFrameLengthStage::LENGTH_16_BIT)
            {
                this->length_ = (this->length_buffer_[0] << 8) + this->length_buffer_[1];
                this->state_ = WebsocketFrameProcessingState::LENGTH_PROCESSED;
            }
            else if (this->length_buffer_.size() == 8 && this->length_stage_ == WebsocketFrameLengthStage::LENGTH_64_BIT)
            {
                this->length_ = (this->length_buffer_[0] << 56) + (this->length_buffer_[1] << 48) + (this->length_buffer_[2] << 40) + (this->length_buffer_[3] << 32) + (this->length_buffer_[4] << 24) + (this->length_buffer_[5] << 16) + (this->length_buffer_[6] << 8) + this->length_buffer_[7];
                this->state_ = WebsocketFrameProcessingState::LENGTH_PROCESSED;
            }
            break;
        default:
            break;
        }
        return this->state_;
    }

    void update_state(WebsocketFrameProcessingState state) override
    {
        this->state_ = state;
    }

    unsigned long long length()
    {
        return this->length_;
    }

    bool masked()
    {
        return this->masked_;
    }

private:
    WebsocketFrameProcessingState state_ = WebsocketFrameProcessingState::PROCESSING_START;
    WebsocketFrameLengthStage length_stage_ = WebsocketFrameLengthStage::LENGTH_7_BIT;
    unsigned long long length_;
    bool masked_;

    std::vector<char> length_buffer_;
};

class WSMaskingKey : public WSStage
{
public:
    WSMaskingKey()
    {
        this->buffer_.reserve(4);
    };

    WebsocketFrameProcessingState push_data(char byte) override
    {
        switch (this->state_)
        {
        case WebsocketFrameProcessingState::LENGTH_PROCESSED:
            this->buffer_.push_back(byte);
            if (this->buffer_.size() == 4)
                this->state_ = WebsocketFrameProcessingState::MASKING_KEY_PROCESSED;
            break;
        default:
            break;
        }

        return this->state_;
    }

    void update_state(WebsocketFrameProcessingState state) override
    {
        this->state_ = state;
    }

    std::vector<char> masking_key()
    {
        return this->buffer_;
    }

private:
    WebsocketFrameProcessingState state_ = WebsocketFrameProcessingState::PROCESSING_START;
    std::vector<char> buffer_;
};

class WebsocketFrameRaw
{
public:
    WebsocketFrameRaw(){};

    WebsocketFrameProcessingState push_data(char *buffer, int buffer_size)
    {
        for (int i = 0; i < buffer_size; i++)
        {
            if (this->state_ == WebsocketFrameProcessingState::PROCESSING_START)
            {
                this->state_ = this->fin_opcode_stage_.push_data(buffer[i]);
                // update states
                this->mask_length_stage_.update_state(this->state_);
                this->masking_key_stage_.update_state(this->state_);
            }
            else if (this->state_ == WebsocketFrameProcessingState::OPCODE_PROCESSED)
            {
                WebsocketFrameProcessingState next = this->mask_length_stage_.push_data(buffer[i]);

                if (next == WebsocketFrameProcessingState::LENGTH_PROCESSED)
                    this->payload_.reserve(this->mask_length_stage_.length());
                this->state_ = next;

                // update states
                this->masking_key_stage_.update_state(this->state_);
            }
            else if (this->state_ == WebsocketFrameProcessingState::MASK_FLAG_PROCESSED)
            {
                WebsocketFrameProcessingState next = this->mask_length_stage_.push_data(buffer[i]);

                if (next == WebsocketFrameProcessingState::LENGTH_PROCESSED)
                    this->payload_.reserve(this->mask_length_stage_.length());
                this->state_ = next;

                // update states
                this->masking_key_stage_.update_state(this->state_);
            }
            else if (this->state_ == WebsocketFrameProcessingState::LENGTH_PROCESSED)
            {
                if (this->mask_length_stage_.masked())
                {
                    this->state_ = this->masking_key_stage_.push_data(buffer[i]);
                }
                else
                {
                    this->state_ = WebsocketFrameProcessingState::MASKING_KEY_PROCESSED;
                    int bytes_left = buffer_size - i;
                    int bytes_to_copy = std::min(bytes_left, (int)this->mask_length_stage_.length() - (int)this->payload_.size());
                    this->payload_.insert(this->payload_.end(), buffer + i, buffer + i + bytes_to_copy);

                    if (this->payload_.size() == this->mask_length_stage_.length())
                    {
                        this->unmask_payload();
                        this->state_ = WebsocketFrameProcessingState::FINISHED_PROCESSING;
                    }
                }

                // update states
                this->masking_key_stage_.update_state(this->state_);
            }
            else if (this->state_ == WebsocketFrameProcessingState::MASKING_KEY_PROCESSED)
            {
                int bytes_left = buffer_size - i;
                int bytes_to_copy = std::min(bytes_left, (int)this->mask_length_stage_.length() - (int)this->payload_.size());
                this->payload_.insert(this->payload_.end(), buffer + i, buffer + i + bytes_to_copy);

                if (this->payload_.size() == this->mask_length_stage_.length())
                {
                    this->unmask_payload();
                    this->state_ = WebsocketFrameProcessingState::FINISHED_PROCESSING;
                    break;
                }
            }
        }

        return this->state_;
    }

    bool finished_processing()
    {
        return this->state_ == WebsocketFrameProcessingState::FINISHED_PROCESSING;
    }

    bool fin()
    {
        return this->fin_opcode_stage_.fin();
    }

    std::vector<bool> reserved()
    {
        std::vector<bool> reserved;
        reserved.push_back((this->fin_opcode_stage_.rsv() >> 2) & 0x1);
        reserved.push_back((this->fin_opcode_stage_.rsv() >> 1) & 0x1);
        reserved.push_back(this->fin_opcode_stage_.rsv() & 0x1);
        return reserved;
    }
    WebsocketOpcode opcode()
    {
        return this->fin_opcode_stage_.opcode();
    }
    bool is_masked()
    {
        return this->mask_length_stage_.masked();
    }
    unsigned long long payload_length()
    {
        return this->mask_length_stage_.length();
    }
    std::vector<char> masking_key()
    {
        return this->masking_key_stage_.masking_key();
    }
    WebsocketFrameProcessingState state()
    {
        return this->state_;
    }
    std::vector<char> payload_;

private:
    WebsocketFrameProcessingState state_ = WebsocketFrameProcessingState::PROCESSING_START;
    WSFinOpcode fin_opcode_stage_;
    WSMaskLength mask_length_stage_;
    WSMaskingKey masking_key_stage_;
    void unmask_payload()
    {
        if (!this->mask_length_stage_.masked())
            return;

        std::vector<char> mask = this->masking_key_stage_.masking_key();
        for (int i = 0; i < this->payload_.size(); i++)
            this->payload_[i] = this->payload_[i] ^ mask[i % 4];
    };
};

class WebsocketFrame
{
public:
    WebsocketFrame(WebsocketFrameRaw &&raw) : raw_(std::move(raw)) {}

    bool fin()
    {
        return this->raw_.fin();
    }

    std::vector<bool> reserved()
    {
        return this->raw_.reserved();
    }

    WebsocketOpcode opcode()
    {
        return this->raw_.opcode();
    }

    bool is_masked()
    {
        return this->raw_.is_masked();
    }

    unsigned long long payload_length()
    {
        return this->raw_.payload_length();
    }

    auto getMoveIterators()
    {
        return std::make_pair(std::make_move_iterator(this->raw_.payload_.begin()), std::make_move_iterator(this->raw_.payload_.end()));
    }

    void clear()
    {
        this->raw_.payload_.clear();
    }

private:
    WebsocketFrameRaw raw_;
};

class WebsocketBuffer
{
public:
    WebsocketBuffer(){};

    void push_data(char *buffer, int buffer_size)
    {
        WebsocketFrameProcessingState state = this->raw_.push_data(buffer, buffer_size);
        if (state == WebsocketFrameProcessingState::FINISHED_PROCESSING)
        {
            this->frames_.push_back(WebsocketFrame(std::move(this->raw_)));
            this->raw_ = WebsocketFrameRaw();
        }
    }

    std::unique_ptr<std::pair<WebsocketOpcode, std::vector<char>>> get_payload()
    {
        if (this->frames_.size() == 0)
            return nullptr;
        std::vector<char> payload = std::vector<char>();

        WebsocketOpcode opcode = this->frames_[0].opcode();

        // Check if fin frame exists
        bool finished_frame_exists = false;
        for (auto frame = this->frames_.begin(); frame != this->frames_.end(); frame++)
        {
            if (frame->fin())
            {
                finished_frame_exists = true;
                break;
            }
        }

        if (!finished_frame_exists)
            return nullptr;

        if (opcode == WebsocketOpcode::CONTINUATION)
            throw std::runtime_error("First frame should not be continuation");

        for (auto frame = this->frames_.begin(); frame != this->frames_.end(); frame++)
        {
            auto iterators = frame->getMoveIterators();
            payload.insert(payload.end(), iterators.first, iterators.second);

            if (frame->fin())
            {
                this->frames_.erase(this->frames_.begin(), frame + 1);
                break;
            }
        }

        return std::make_unique<std::pair<WebsocketOpcode, std::vector<char>>>(opcode, std::move(payload));
    }

private:
    WebsocketFrameRaw raw_;
    std::vector<WebsocketFrame> frames_;
};

std::unique_ptr<std::vector<char>> get_websocket_frame_buffer(WebsocketOpcode opcode, std::string payload, bool fin = true)
{
    std::unique_ptr<std::vector<char>> buffer = std::make_unique<std::vector<char>>();

    buffer->push_back((fin << 7) + ((char)opcode & 0xF));

    unsigned long long payload_length = payload.size();

    if (payload_length < 126)
    {
        buffer->push_back(payload_length);
    }
    else if (payload_length < 65536)
    {
        buffer->push_back(126);
        buffer->push_back((payload_length >> 8) & 0xFF);
        buffer->push_back(payload_length & 0xFF);
    }
    else
    {
        buffer->push_back(127);
        buffer->push_back((payload_length >> 56) & 0xFF);
        buffer->push_back((payload_length >> 48) & 0xFF);
        buffer->push_back((payload_length >> 40) & 0xFF);
        buffer->push_back((payload_length >> 32) & 0xFF);
        buffer->push_back((payload_length >> 24) & 0xFF);
        buffer->push_back((payload_length >> 16) & 0xFF);
        buffer->push_back((payload_length >> 8) & 0xFF);
        buffer->push_back(payload_length & 0xFF);
    }

    buffer->reserve(buffer->size() + payload_length);
    buffer->insert(buffer->end(), payload.begin(), payload.end());

    return buffer;
}
class WebsocketServerThread : public BaseServerThread,
                              public IAudioListener
{
public:
    WebsocketServerThread(std::unique_ptr<ClientConnectionMetadata> connectionMetadata, std::weak_ptr<BaseWebsocketServer> server, std::weak_ptr<AudioQueueRwLock> queue) : connectionMetadata_(std::move(connectionMetadata)), server_(server), queue_(queue)
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
    std::weak_ptr<AudioQueueRwLock> queue_;
    WebsocketBuffer buffer_;

    void process_payload(std::unique_ptr<std::pair<WebsocketOpcode, std::vector<char>>> payload);
};

void WebsocketServerThread::start_handling()
{
    std::cout << "Upgraded to websocket" << std::endl;
    char buffer[1024] = {0};

    while (this->yeet_flag == false)
    {
        memset(buffer, 0, sizeof(buffer));

        int bytes_read = read(this->connectionMetadata_->get(), buffer, sizeof(buffer));
        if (bytes_read <= 0)
        {
            this->yeet_flag = true;
            break;
        }

        this->buffer_.push_data(buffer, bytes_read);

        auto payload = this->buffer_.get_payload();
        if (payload == nullptr)
            continue;

        this->process_payload(std::move(payload));
    }

    this->yeet_flag = true;
}

void WebsocketServerThread::process_payload(std::unique_ptr<std::pair<WebsocketOpcode, std::vector<char>>> payload)
{
    if (payload->first == WebsocketOpcode::TEXT)
    {
        std::string payload_string(payload->second.begin(), payload->second.end());
        try
        {
            nlohmann::json json = nlohmann::json::parse(payload_string);
            if (json["type"] == "command")
            {
                if (json["command"] == "skip")
                {
                    this->queue_.lock()->lock_write();
                    this->queue_.lock()->get_queue().skip_audio_file(int(json["idx"]));
                    this->queue_.lock()->unlock_write();
                }

                else if (json["command"] == "swap")
                {
                    this->queue_.lock()->lock_write();
                    this->queue_.lock()->get_queue().swap_audio_files(int(json["idx1"]), int(json["idx2"]));
                    this->queue_.lock()->unlock_write();
                }

                else if (json["command"] == "cplay")
                {
                    this->queue_.lock()->lock_write();
                    this->queue_.lock()->get_queue().cplay();
                    this->queue_.lock()->unlock_write();
                }

                else if (json["command"] == "get_song")
                {
                    std::shared_ptr<AudioFile> file = std::make_shared<AudioFile>("Captain.mp3");
                    this->queue_.lock()->lock_write();
                    this->queue_.lock()->get_queue().push(file);
                    this->queue_.lock()->unlock_write();
                }
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << e.what() << '\n';
        }
    }
    else if (payload->first == WebsocketOpcode::CLOSE)
    {
        this->yeet_flag = true;
    }
}

void WebsocketServerThread::on_audio_block(std::shared_ptr<AudioBlock> block)
{
    nlohmann::json json;
    json["audio_block"]["duration"] = block->duration;
    json["audio_block"]["rate"] = block->sampling_rate;
    json["audio_block"]["data"] = block->base64();

    std::unique_ptr<std::vector<char>> buffer = get_websocket_frame_buffer(WebsocketOpcode::TEXT, json.dump(), true);
    ssize_t result = write(this->connectionMetadata_->get(), buffer->data(), buffer->size());
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
    std::unique_ptr<std::vector<char>> buffer = get_websocket_frame_buffer(WebsocketOpcode::TEXT, queue.dump(), true);
    write(this->connectionMetadata_->get(), buffer->data(), buffer->size());
}

#endif // !WEBSOCKET_SERVER_THREAD_H