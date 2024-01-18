#pragma once
#ifndef WEBSOCKET_SERVER_INTERFACE_H
#define WEBSOCKET_SERVER_INTERFACE_H

#include "server_thread_interface.hpp"
#include <memory>

class WebsocketServerThread;

class BaseWebsocketServer
{
public:
    virtual void upgrade(std::shared_ptr<WebsocketServerThread> serverThread) = 0;
};

#endif // !WEBSOCKET_SERVER_INTERFACE_H