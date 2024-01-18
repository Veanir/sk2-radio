#pragma once

#include <mpg123.h>
#include <memory>
#include <vector>
#include <iostream>

class AudioBlock
{
public:
    AudioBlock(unsigned char *data, size_t size, double duration, int sampling_rate);
    ~AudioBlock();

    unsigned char *data;
    size_t size;
    double duration;
    int sampling_rate;

    std::string base64();
    std::vector<unsigned char> data_vector();
};

class AudioFile
{
public:
    AudioFile(const char *filename);
    ~AudioFile();
    std::vector<std::shared_ptr<AudioBlock>> fetchAudioBlocks();
    std::shared_ptr<AudioBlock> fetchNextAudioBlock();
    std::shared_ptr<AudioBlock> fetchCurrentAudioBlock();
    std::string get_filename() { return this->m_filename; }
    long get_sampling_rate() { return this->m_rate; }
    int get_channels() { return this->m_channels; }
    int get_encoding() { return this->m_encoding; }
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
