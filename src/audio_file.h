#include <mpg123.h>
#include <memory>
#include <vector>
#include <iostream>

class AudioBlock
{
public:
    AudioBlock(unsigned char *data, size_t size);
    ~AudioBlock();

    unsigned char *data;
    size_t size;
};

class AudioFile
{
public:
    AudioFile(const char *filename);
    ~AudioFile();
    std::vector<std::unique_ptr<AudioBlock>> fetchAudio();

private:
    mpg123_handle *m_handle;
    size_t m_size;
    long m_rate;
    int m_channels, m_encoding;

    std::vector<std::unique_ptr<AudioBlock>> m_blocks;
};
