#include "audio_queue.h"
#include "server_master.h"
#include <mpg123.h>
#include "audio_file.h"
#include "audio_queue.h"
#include <memory>
#include <vector>

#include <iostream>
#include <thread>

int main()
{
    mpg123_init();

    AudioFile file("/home/eryk_stec/Studia/Semestr-5/sieci-komputerowe-2/projekt-radio/server/Captain.mp3");

    std::shared_ptr<AudioQueueRwLock> queue = std::make_shared<AudioQueueRwLock>();

    queue->lock_write();
    queue->get_queue().push(std::make_shared<AudioFile>(file));
    queue->unlock_write();

    while (true)
    {
        queue->lock_write();
        queue->get_queue().update();
        queue->unlock_write();
    }

    return 0;
}