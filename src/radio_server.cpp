#include <mpg123.h>
#include "audio_file.h"
#include <memory>
#include <vector>

#include <iostream>

int main()
{
    mpg123_init();
    AudioFile file("./Guy.mp3");
    std::vector<std::unique_ptr<AudioBlock>> blocks = file.fetchAudio();
    for (auto &block : blocks)
    {
        std::cout << block->size << std::endl;
    }

    return 0;
}