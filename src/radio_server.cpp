#include <mpg123.h>
#include "audio_file.h"
#include "audio_queue.h"
#include <memory>
#include <vector>

#include <iostream>

int main()
{
    mpg123_init();

    AudioFile file("./Guy.mp3");

    std::shared_ptr<AudioQueueRwLock> queue = std::make_shared<AudioQueueRwLock>();

    while (true)
    {
        queue->lock_write();
        queue->get_queue().update();
        queue->unlock_write();
    }

    return 0;
}