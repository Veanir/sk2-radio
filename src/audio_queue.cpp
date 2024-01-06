#include "audio_file.h"
#include "audio_queue.h"
#include <chrono>
#include <functional>
#include <memory>
#include <vector>
#include <mutex>
#include <condition_variable>
AudioQueue::AudioQueue()
{
    this->audio_block_start_time = std::chrono::high_resolution_clock::now();
}

AudioQueue::~AudioQueue()
{
}

void AudioQueue::push(std::shared_ptr<AudioFile> file)
{
    this->audio_files.push_back(file);
}

void AudioQueue::subscribe(std::function<void(std::shared_ptr<AudioBlock>)> callback)
{
    this->callbacks.push_back(callback);
}

void AudioQueue::update()
{
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - this->audio_block_start_time).count();

    if (this->audio_files.size() > 0)
    {
        auto file = this->audio_files[0];
        auto block = file->fetchNextAudioBlock();

        if (block != NULL)
        {
            if (duration >= block->duration * 1000)
            {
                this->update_listeners(block);
                this->audio_block_start_time = std::chrono::high_resolution_clock::now();
            }
        }
        else
        {
            this->audio_files.erase(this->audio_files.begin());
        }
    }
}

void AudioQueue::update_listeners(std::shared_ptr<AudioBlock> block)
{
    for (auto callback : this->callbacks)
    {
        callback(block);
    }
}

void AudioQueueRwLock::lock_write()
{
    std::unique_lock<std::mutex> lock(this->mutex);
    this->write_condition.wait(lock, [this]
                               { return !this->writer && this->readers == 0; });
    this->writer = true;
}

void AudioQueueRwLock::unlock_write()
{
    std::unique_lock<std::mutex> lock(this->mutex);
    this->writer = false;
    this->read_condition.notify_all();
}

void AudioQueueRwLock::lock_read()
{
    std::unique_lock<std::mutex> lock(this->mutex);
    this->read_condition.wait(lock, [this]
                              { return !this->writer; });
    this->readers++;
}

void AudioQueueRwLock::unlock_read()
{
    std::unique_lock<std::mutex> lock(this->mutex);
    this->readers--;
    if (this->readers == 0)
        this->write_condition.notify_one();
}

AudioQueue &AudioQueueRwLock::get_queue()
{
    return this->queue;
}