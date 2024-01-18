#include "audio_file.h"
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

AudioFile::AudioFile(const char *filename)
{
    this->m_handle = mpg123_new(NULL, NULL);
    mpg123_open(this->m_handle, filename);

    mpg123_getformat(this->m_handle, &this->m_rate, &this->m_channels, &this->m_encoding);

    this->m_size = mpg123_outblock(this->m_handle);

    this->blocks_count = 0;
    this->position = 0;
    this->m_filename = filename;

    while (true)
    {
        unsigned char *data = new unsigned char[this->m_size];
        size_t done;
        if (mpg123_read(this->m_handle, data, this->m_size, &done) == MPG123_OK)
        {
            size_t samples = done / (this->m_channels * mpg123_encsize(this->m_encoding));
            double duration = (double)samples / (double)this->m_rate;
            this->m_blocks.push_back(std::shared_ptr<AudioBlock>(new AudioBlock(data, done, duration, this->m_rate)));

            this->blocks_count++;
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

std::vector<std::shared_ptr<AudioBlock>> AudioFile::fetchAudioBlocks()
{
    std::vector<std::shared_ptr<AudioBlock>> blocks;
    blocks.swap(this->m_blocks);
    return blocks;
}

std::shared_ptr<AudioBlock> AudioFile::fetchNextAudioBlock()
{
    if (this->position < this->blocks_count)
        return this->m_blocks[this->position++];
    else
        return NULL;
}

std::shared_ptr<AudioBlock> AudioFile::fetchCurrentAudioBlock()
{
    if (this->position < this->blocks_count)
        return this->m_blocks[this->position];
    else
        return NULL;
}

AudioBlock::AudioBlock(unsigned char *data, size_t size, double duration, int sampling_rate)
{
    this->data = data;
    this->size = size;
    this->duration = duration;
    this->sampling_rate = sampling_rate;
}

AudioBlock::~AudioBlock()
{
    delete[] this->data;
}

std::string AudioBlock::base64()
{
    BIO *bio, *b64;
    BUF_MEM *bufferPtr;

    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bio = BIO_push(b64, bio);

    BIO_write(bio, this->data, this->size);
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &bufferPtr);

    std::string encoded(bufferPtr->data, bufferPtr->length);
    BIO_free_all(bio);

    return encoded;
}

std::vector<unsigned char> AudioBlock::data_vector()
{
    std::vector<unsigned char> data;
    data.resize(this->size);
    for (size_t i = 0; i < this->size; i++)
        data[i] = this->data[i];

    return data;
}