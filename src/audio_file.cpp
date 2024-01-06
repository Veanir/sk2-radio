#include "audio_file.h"

AudioFile::AudioFile(const char *filename)
{
    this->m_handle = mpg123_new(NULL, NULL);
    if (mpg123_open(this->m_handle, filename) != MPG123_OK)
    {
        std::cout << "Failed to open file" << std::endl;
        return;
    }

    mpg123_getformat(this->m_handle, &this->m_rate, &this->m_channels, &this->m_encoding);

    this->m_size = mpg123_outblock(this->m_handle);

    std::cout << "AudioFile m_size -> " << this->m_size << std::endl;

    while (true)
    {
        unsigned char *data = new unsigned char[this->m_size];
        size_t done;
        if (mpg123_read(this->m_handle, data, this->m_size, &done) == MPG123_OK)
        {
            this->m_blocks.push_back(std::unique_ptr<AudioBlock>(new AudioBlock(data, done)));
        }
        else
        {
            delete[] data;
            break;
        }
    }
}

AudioFile::~AudioFile()
{
    mpg123_close(this->m_handle);
}

std::vector<std::unique_ptr<AudioBlock>> AudioFile::fetchAudio()
{
    std::vector<std::unique_ptr<AudioBlock>> blocks;
    blocks.swap(this->m_blocks);
    return blocks;
}

AudioBlock::AudioBlock(unsigned char *data, size_t size)
{
    this->data = data;
    this->size = size;
}

AudioBlock::~AudioBlock()
{
    delete[] this->data;
}