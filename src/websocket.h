#pragma once

#include <vector>
#include <memory>
#include <cstring>
#include <stdexcept>

typedef unsigned long size_t;

enum class WebSocketFrameOpcode : unsigned char
{
    CONTINUATION = 0x0,
    TEXT = 0x1,
    BINARY = 0x2,
    CLOSE = 0x8,
    PING = 0x9,
    PONG = 0xA
};

class WebSocketFramePayload
{
public:
    WebSocketFramePayload(unsigned char *data, size_t size, bool mask) : data(data), size(size), mask(mask){};

    unsigned char *data;
    size_t size;
    bool mask;

    void unmask();
};

void WebSocketFramePayload::unmask()
{
    if (this->mask)
    {
        for (size_t i = 0; i < this->size; i++)
        {
            this->data[i] = this->data[i] ^ this->data[i % 4];
        }

        this->mask = false;
    }
}

class WebSocketFrame
{
public:
    WebSocketFrame(unsigned char *data, size_t size, bool fin, bool mask, unsigned char opcode);

    bool fin;
    WebSocketFramePayload payload;
    WebSocketFrameOpcode opcode;
};

WebSocketFrame::WebSocketFrame(unsigned char *data, size_t size, bool fin, bool mask, unsigned char opcode) : payload(data, size, mask)
{
    this->fin = fin;
    this->opcode = (WebSocketFrameOpcode)opcode;
}

class WebSocketFrameParser
{
public:
    WebSocketFrameParser();

    void parse(unsigned char *data, size_t size);
    WebSocketFrame get_frame();

    std::vector<std::unique_ptr<WebSocketFrame>> frames;
};

void WebSocketFrameParser::parse(unsigned char *data, size_t size)
{
    if (size == 0)
        return;

    int offset = 0;
    while (true)
    {
        bool fin = (data[offset] & 0x80) != 0;
        unsigned char opcode = data[offset] & 0x0F;
        offset++;
        bool mask = (data[offset] & 0x80) != 0;
        size_t payload_size = data[offset] & 0x7F;

        if (payload_size == 126)
        {
            payload_size = (data[offset + 1] << 8) | data[offset + 2];
            offset += 2;
        }
        else if (payload_size == 127)
        {
            throw std::runtime_error("Payload size of 127 is not supported");
        }

        offset++;

        unsigned char *payload_data = data + offset;

        if (payload_size > size - offset)
            throw std::runtime_error("Payload size is larger than frame size");

        offset += payload_size;

        std::unique_ptr<WebSocketFrame> frame = std::unique_ptr<WebSocketFrame>(new WebSocketFrame(payload_data, payload_size, fin, mask, opcode));

        this->frames.push_back(std::move(frame));

        if (offset == size)
            break;
    }
}
