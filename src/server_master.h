#pragma once
#include "audio_queue.h"
#include "server_node.h"

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

typedef struct sockaddr_in sockaddr_in;

class ServerMaster
{
public:
    ServerMaster(int port, std::shared_ptr<AudioQueueRwLock> queue);

    void start_listening();

private:
    std::shared_ptr<AudioQueueRwLock> queue;
    sockaddr_in addr;
    int socket_fd;
};

ServerMaster::ServerMaster(int port, std::shared_ptr<AudioQueueRwLock> queue) : queue(queue)
{
    this->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (this->socket_fd == -1)
        throw std::runtime_error("Failed to create socket");

    memset(&this->addr, 0, sizeof(sockaddr_in));
    this->addr.sin_family = AF_INET;
    this->addr.sin_port = htons(port);
    this->addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(this->socket_fd, (sockaddr *)&this->addr, sizeof(sockaddr_in)) == -1)
        throw std::runtime_error("Failed to bind socket");

    if (listen(this->socket_fd, 10) == -1)
        throw std::runtime_error("Failed to listen on socket");
}

void ServerMaster::start_listening()
{
    while (true)
    {
        sockaddr_in client_addr;
        socklen_t client_addr_size = sizeof(sockaddr_in);
        int client_fd = accept(this->socket_fd, (sockaddr *)&client_addr, &client_addr_size);

        if (client_fd == -1)
            throw std::runtime_error("Failed to accept connection");

        // set socket to non-blocking
        int flags = fcntl(client_fd, F_GETFL, 0);
        fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

        std::cout << "Accepted connection from " << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << std::endl;

        std::unique_ptr<ServerNode> node = std::unique_ptr<ServerNode>(new ServerNode(client_fd, this->queue));

        std::thread([node = std::move(node)]() mutable
                    { node->start_listening(); })
            .detach();
    }
}
