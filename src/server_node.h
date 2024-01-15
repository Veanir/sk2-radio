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

class ServerNode : public IAudioListener
{
public:
    ServerNode(int client_fd, std::shared_ptr<AudioQueueRwLock> queue);
    void start_listening();

    void on_audio_block(std::shared_ptr<AudioBlock> block) override;
    void on_queue_change(std::string) override;

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
    std::cout << "Sending audio block" << std::endl;
    std::ostringstream http_message;
    if (block->data && block->size > 0)
        http_message.write(reinterpret_cast<const char *>(block->data), block->size);

    std::string message = http_message.str();
    write(this->client_fd, message.c_str(), message.size());
}

void ServerNode::on_queue_change(std::string queue)
{
    std::cout << "Sending queue change" << std::endl;
    std::ostringstream http_message;

    http_message << queue;

    std::string message = http_message.str();
    write(this->client_fd, message.c_str(), message.size());
}

void ServerNode::start_listening()
{
    std::cout << "Client connected" << std::endl;
    llhttp_init(&this->parser, HTTP_REQUEST, &this->settings);

    this->queue->lock_read();
    std::string queue_info = this->queue->get_queue().queue_info();
    this->queue->unlock_read();

    this->on_queue_change(queue_info);

    char buffer[4096];
    int bytes_read = 0;
    while (true)
    {
        bytes_read = read(this->client_fd, buffer, sizeof(buffer));
        if (bytes_read == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                continue;
            else
            {
                std::cerr << "Error reading from socket" << std::endl;
                break;
            }
        }
        else if (bytes_read == 0)
            break;
        llhttp_execute(&this->parser, buffer, bytes_read);
    }

    close(this->client_fd);
    std::cout << "Client disconnected" << std::endl;
}