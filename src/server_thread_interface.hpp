#pragma once
#ifndef SERVER_THREAD_INTERFACE_H
#define SERVER_THREAD_INTERFACE_H

class BaseServerThread
{
public:
    virtual void start_handling() = 0;
    virtual bool yeet() = 0;
    virtual ~BaseServerThread() {}
};

#endif // !SERVER_THREAD_INTERFACE_H