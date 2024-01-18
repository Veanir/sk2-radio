#include "server.hpp"
#include <memory>
#include <thread>

#include "audio/audio_queue.h"
#include "audio/audio_file.h"

#include <signal.h>
#include <unistd.h>

int main()
{
    struct sigaction sigpipe_action;
    sigpipe_action.sa_handler = SIG_IGN; // Ignorowanie sygnału
    sigemptyset(&sigpipe_action.sa_mask);
    sigpipe_action.sa_flags = 0;
    sigaction(SIGPIPE, &sigpipe_action, NULL);

    std::shared_ptr<AudioQueueRwLock> queue = std::make_shared<AudioQueueRwLock>();

    std::shared_ptr<Server> server = Server::Create(3030, queue);
    if (server == nullptr)
        return 1;

    std::thread server_thread(&Server::start_listening, server);
    server_thread.detach();

    std::shared_ptr<AudioFile> file = std::make_shared<AudioFile>("Captain.mp3");
    std::shared_ptr<AudioFile> file2 = std::make_shared<AudioFile>("Guy.mp3");

    queue->lock_write();
    queue->get_queue().push(file2);
    queue->get_queue().push(file);
    queue->unlock_write();

    while (true)
    {
        queue->lock_write();
        queue->get_queue().update();
        queue->unlock_write();
    }

    return 0;
}