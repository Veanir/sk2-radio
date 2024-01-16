#pragma once
#ifndef SERVER_H
#define SERVER_H

#define DEBUG

#include "connection_utilities.hpp"
#include "server_thread_interface.hpp"
#include "websocket_server_interface.hpp"
#include "server_thread.hpp"
#include "websocket_server_thread.hpp"

// Network
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// Standard
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <memory>
#include <vector>

// Error handling
#include <stdexcept>
#include <cerrno>
#include <cstring>

// FD Polling
#include <fcntl.h>

typedef struct sockaddr_in sockaddr_in;

class ServerSocket
{
public:
    ServerSocket(int port) : socketRAII_(socket(AF_INET, SOCK_STREAM, 0))
    {
        if (socketRAII_.get() < 0)
            throw std::runtime_error("Could not create socket: " + std::string(strerror(errno)));

        this->address.sin_family = AF_INET;
        this->address.sin_addr.s_addr = INADDR_ANY;
        this->address.sin_port = htons(port);
        memset(this->address.sin_zero, '\0', sizeof(this->address.sin_zero));

        if (bind(this->socketRAII_.get(), (struct sockaddr *)&this->address, sizeof(this->address)) < 0)
            throw std::runtime_error("Could not bind socket: " + std::string(strerror(errno)));
    }

protected:
    SocketRAII socketRAII_;
    sockaddr_in address;
};

class Server : private ServerSocket, public BaseWebsocketServer
{
public:
    Server(int port);

    static std::shared_ptr<Server> Create(int port);

    std::unique_ptr<ClientConnectionMetadata> checkSocket();

    void start_listening();

    void upgrade(std::unique_ptr<BaseServerThread> thread) override;

    using ServerSocket::address;
    using ServerSocket::socketRAII_;

private:
    std::vector<std::unique_ptr<BaseServerThread>> threads_;
    std::weak_ptr<Server> self_;
};

Server::Server(int port) : ServerSocket(port)
{
    if (listen(this->socketRAII_.get(), 10) < 0)
        throw std::runtime_error("Could not listen on socket");
}

std::shared_ptr<Server> Server::Create(int port)
{
    try
    {
        std::shared_ptr<Server> server = std::shared_ptr<Server>(new Server(port));
        server->self_ = server;
        return server;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error creating Sever object: " << e.what() << '\n';
        return nullptr;
    }
};

std::unique_ptr<ClientConnectionMetadata> Server::checkSocket()
{
    struct sockaddr_in clientAddress;
    socklen_t clientAddressLength = sizeof(clientAddress);
    memset(&clientAddress, 0, sizeof(clientAddress));
    int clientSocket = accept(this->socketRAII_.get(), (struct sockaddr *)&clientAddress, &clientAddressLength);

    if (clientSocket == -1)
        throw std::runtime_error("Could not accept connection: " + std::string(strerror(errno)));

    return std::make_unique<ClientConnectionMetadata>(clientSocket, clientAddress);
}

void Server::start_listening()
{
    while (true)
    {

        try
        {

            for (auto it = this->threads_.begin(); it != this->threads_.end();)
            {
                if ((*it)->yeet())
                {
                    it = this->threads_.erase(it);
                    continue;
                }
                it++;
            }
            std::unique_ptr<ClientConnectionMetadata> client = this->checkSocket();
#ifdef DEBUG
            std::cout << "Connection accepted from " << inet_ntoa(client->address.sin_addr) << ":" << ntohs(client->address.sin_port) << '\n';
#endif

            this->threads_.emplace_back(std::make_unique<ServerThread>(std::move(client), this->self_));
        }
        catch (const std::exception &e)
        {
            std::cerr << "Failed to accept connection: " << e.what() << '\n';
        }
    }
}

void Server::upgrade(std::unique_ptr<BaseServerThread> thread)
{
    this->threads_.emplace_back(std::move(thread));
}

#endif // !SERVER_H