#include "server.hpp"
#include <memory>

int main()
{
    std::shared_ptr<Server> server = Server::Create(3030);

    if (server == nullptr)
        return 1;

    server->start_listening();
    return 0;
}