#include "audio_file.h"
#include "audio_queue.h"
#include <chrono>
#include <functional>
#include <memory>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <string>
AudioQueue::AudioQueue()
{
    this->audio_block_start_time = std::chrono::high_resolution_clock::now();
}

void AudioQueue::push(std::shared_ptr<AudioFile> file)
{
    this->audio_files.push_back(file);
}

void AudioQueue::subscribe(std::weak_ptr<IAudioListener> listener)
{
    this->listeners.push_back(listener);
}

void AudioQueue::update()
{
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - this->audio_block_start_time).count();
    if (this->audio_files.size() == 0)
    {
        return;
    }
    auto file = this->audio_files[0];
    auto current_block = file->fetchCurrentAudioBlock();
    if (current_block == nullptr)
    {
        this->audio_files.erase(this->audio_files.begin());
        this->update_listeners_queue(this->queue_info());
        return;
    }

    if (duration >= current_block->duration * 1000)
    {
        file->fetchNextAudioBlock();
        auto block = file->fetchCurrentAudioBlock();
        if (block == NULL)
        {
            this->audio_files.erase(this->audio_files.begin());
            this->update_listeners_queue(this->queue_info());
            return;
        }

        this->update_listeners_audio(block);
        this->audio_block_start_time = std::chrono::high_resolution_clock::now();
    }
}

void AudioQueue::update_listeners_audio(std::shared_ptr<AudioBlock> block)
{
    for (auto it = this->listeners.begin(); it != this->listeners.end();)
    {
        auto listener = it->lock();
        if (listener)
        {
            listener->on_audio_block(block);
            ++it;
        }
        else
        {
            it = this->listeners.erase(it);
        }
    }
}

void AudioQueue::update_listeners_queue(std::string queue)
{
    for (auto it = this->listeners.begin(); it != this->listeners.end();)
    {
        auto listener = it->lock();
        if (listener)
        {
            listener->on_queue_change(queue);
            ++it;
        }
        else
        {
            it = this->listeners.erase(it);
        }
    }
}

std::string AudioQueue::queue_info()
{
    std::string queue;
    queue += "{ \"queue\": [";

    for (auto it = this->audio_files.begin(); it != this->audio_files.end(); ++it)
    {
        queue += "\"";
        queue += (*it)->get_filename();
        queue += "\"";
        if (it != this->audio_files.end() - 1)
            queue += ", ";
    }
    queue += "], \"current\": { \"filename\": \"";
    queue += this->audio_files[0]->get_filename();
    queue += "\", \"sampling_rate\" : ";
    queue += std::to_string(this->audio_files[0]->get_sampling_rate());
    queue += ", \"channels\" : ";
    queue += std::to_string(this->audio_files[0]->get_channels());
    queue += ", \"encoding\" : ";
    queue += std::to_string(this->audio_files[0]->get_encoding());
    queue += " } }";
    return queue;
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
    this->write_condition.notify_one();
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