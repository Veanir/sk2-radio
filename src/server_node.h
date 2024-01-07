#pragma once
#include "audio_queue.h"
#include "../llhttp/build/llhttp.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <thread>
#include <iostream>
#include <fcntl.h>
#include <errno.h>

#include <sstream>
#include <string>

class ServerNode : IAudioListener
{
public:
    ServerNode(int client_fd, std::shared_ptr<AudioQueueRwLock> queue);
    void start_listening();

    void on_audio_block(std::shared_ptr<AudioBlock> block) override;
    void on_queue_change(std::vector<std::string>) override;

private:
    int client_fd;
    llhttp_t parser;
    llhttp_settings_t settings;
    std::shared_ptr<AudioQueueRwLock> queue;
};

int on_header_field_complete(llhttp_t *parser)
{
    std::cout << "on_header_field_complete" << std::endl;
    return 0;
}

ServerNode::ServerNode(int client_fd, std::shared_ptr<AudioQueueRwLock> queue)
{
    this->client_fd = client_fd;
    this->queue = queue;

    llhttp_settings_init(&this->settings);

    this->settings.on_header_field_complete = on_header_field_complete;

    llhttp_init(&this->parser, HTTP_REQUEST, &this->settings);
}

void ServerNode::on_audio_block(std::shared_ptr<AudioBlock> block)
{
    std::ostringstream http_message;

    http_message << "POST /audio HTTP/1.1\r\n";
    http_message << "Content-Type: audio/pcm\r\n";
    http_message << "Content-Length: " + std::to_string(block->size) + "\r\n";
    http_message << "\r\n";

    if (block->data && block->size > 0)
        http_message.write(reinterpret_cast<const char *>(block->data), block->size);

    std::string message = http_message.str();
    write(this->client_fd, message.c_str(), message.size());
}

void ServerNode::on_queue_change(std::vector<std::string> queue)
{
}

void ServerNode::start_listening()
{
    llhttp_init(&this->parser, HTTP_REQUEST, &this->settings);

    char buffer[4096];
    int bytes_read = 0;
    while ((bytes_read = read(this->client_fd, buffer, 4096)) > 0)
    {
        llhttp_execute(&this->parser, buffer, bytes_read);
    }

    close(this->client_fd);
}