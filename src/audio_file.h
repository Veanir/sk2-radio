#pragma once

#include <mpg123.h>
#include <memory>
#include <vector>
#include <iostream>

class AudioBlock
{
public:
    AudioBlock(unsigned char *data, size_t size, double duration);
    ~AudioBlock();

    unsigned char *data;
    size_t size;
    double duration;
};

class AudioFile
{
public:
    AudioFile(const char *filename);
    ~AudioFile();
    std::vector<std::shared_ptr<AudioBlock>> fetchAudioBlocks();
    std::shared_ptr<AudioBlock> fetchNextAudioBlock();
    std::string get_filename() { return this->m_filename; }
    void rewind();

private:
    const char *m_filename;
    mpg123_handle *m_handle;
    size_t m_size;
    long m_rate;
    int m_channels, m_encoding;

    size_t blocks_count;
    size_t position;

    std::vector<std::shared_ptr<AudioBlock>> m_blocks;
};
