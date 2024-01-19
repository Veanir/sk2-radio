#include "audio_file.h"
#include "audio_queue.h"
#include <chrono>
#include <functional>
#include <memory>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <string>

#include <nlohmann/json.hpp>

AudioQueue::AudioQueue()
{
    this->audio_block_start_time = std::chrono::high_resolution_clock::now();
}

void AudioQueue::push(std::shared_ptr<AudioFile> file)
{
    this->audio_files.push_back(file);
    this->update_listeners_queue(this->queue_info());
}

void AudioQueue::subscribe(std::weak_ptr<IAudioListener> listener)
{
    this->listeners.push_back(listener);
    listener.lock()->on_queue_change(this->queue_info());
}

void AudioQueue::cplay()
{
    this->is_playing = !this->is_playing;
    if (this->is_playing)
        this->audio_block_start_time = std::chrono::high_resolution_clock::now();

    this->update_listeners_queue(this->queue_info());
}

void AudioQueue::update()
{
    if (!this->is_playing)
        return;
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

        if (listener == nullptr || listener->yeet())
        {
            it = this->listeners.erase(it);
            continue;
        }
        listener->on_audio_block(block);
        ++it;
    }
}

void AudioQueue::update_listeners_queue(nlohmann::json queue)
{
    for (auto it = this->listeners.begin(); it != this->listeners.end();)
    {
        auto listener = it->lock();

        if (listener == nullptr || listener->yeet())
        {
            it = this->listeners.erase(it);
            continue;
        }

        listener->on_queue_change(queue);
        ++it;
    }
}

nlohmann::json AudioQueue::queue_info()
{
    nlohmann::json json;
    json["metadata"]["is_playing"] = this->is_playing;
    json["metadata"]["queue"]["size"] = this->audio_files.size();
    json["metadata"]["queue"]["files"] = nlohmann::json::array();
    for (int i = 0; i < this->audio_files.size(); i++)
    {
        json["metadata"]["queue"]["files"][i] = this->audio_files[i]->get_filename();
    }

    if (this->audio_files.size() == 0)
        return json;

    auto file = this->audio_files[0];
    json["metadata"]["current"]["filename"] = file->get_filename();
    json["metadata"]["current"]["sampling_rate"] = file->get_sampling_rate();
    json["metadata"]["current"]["channels"] = file->get_channels();
    json["metadata"]["current"]["encoding"] = file->get_encoding();

    return json;
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

void AudioQueue::skip_audio_file(int index)
{
    if (index < 0 || index >= this->audio_files.size())
        return;

    this->audio_files.erase(this->audio_files.begin() + index);
    this->update_listeners_queue(this->queue_info());
}

void AudioQueue::swap_audio_files(int index1, int index2)
{
    if (index1 < 0 || index1 >= this->audio_files.size())
        return;
    if (index2 < 0 || index2 >= this->audio_files.size())
        return;
    if (index1 == index2)
        return;
    std::swap(this->audio_files[index1], this->audio_files[index2]);
    this->update_listeners_queue(this->queue_info());
}