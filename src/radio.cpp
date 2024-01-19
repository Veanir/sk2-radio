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
    std::shared_ptr<AudioFile> file3 = std::make_shared<AudioFile>("Africa.mp3");
    std::shared_ptr<AudioFile> file4 = std::make_shared<AudioFile>("Rainbow.mp3");
    std::shared_ptr<AudioFile> file5 = std::make_shared<AudioFile>("Rick.mp3");
    std::shared_ptr<AudioFile> file6 = std::make_shared<AudioFile>("Take.mp3");

    queue->lock_write();
    queue->get_queue().push(file);
    queue->get_queue().push(file2);
    queue->get_queue().push(file3);
    queue->get_queue().push(file4);
    queue->get_queue().push(file5);
    queue->get_queue().push(file6);
    queue->unlock_write();

    while (true)
    {
        queue->lock_write();
        queue->get_queue().update();
        queue->unlock_write();
    }

    return 0;
}

void AudioQueue::rewind()
{
    if (this->audio_files.size() == 0)
        return;
    auto file = this->audio_files[0];
    file->rewind();
    this->update_listeners_queue(this->queue_info());
}