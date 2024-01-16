#pragma once
#ifndef CONNECTION_UTILITIES_H
#define CONNECTION_UTILITIES_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <string.h>
#include <iostream>

typedef struct sockaddr_in sockaddr_in;

class SocketRAII
{
public:
    explicit SocketRAII(int fd) : fd_(fd) {}
    ~SocketRAII()
    {
        if (fd_ >= 0)
            close(fd_);
    }

    SocketRAII(const SocketRAII &) = delete;
    SocketRAII &operator=(const SocketRAII &) = delete;

    int get() const { return fd_; }

private:
    int fd_;
};

class ClientConnectionMetadata : public SocketRAII
{
public:
    ClientConnectionMetadata(int fd, sockaddr_in address) : SocketRAII(fd), address(address) {}

    sockaddr_in address;
};
#endif // !CONNECTION_UTILITIES_H