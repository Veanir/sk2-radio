#pragma once

#include "audio_file.h"
#include <chrono>
#include <functional>
#include <memory>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <string.h>

#include <nlohmann/json.hpp>

#include "../server_thread_interface.hpp"

class IAudioListener : public Object
{
public:
    virtual void on_audio_block(std::shared_ptr<AudioBlock> block) = 0;
    virtual void on_queue_change(nlohmann::json queue) = 0;
    virtual bool yeet() = 0;
};

class AudioQueue
{
public:
    AudioQueue();

    void push(std::shared_ptr<AudioFile> file);
    void subscribe(std::weak_ptr<IAudioListener> listener);
    void update();

    void update_listeners_audio(std::shared_ptr<AudioBlock> block);
    void update_listeners_queue(nlohmann::json queue);
    void skip_audio_file(int index);
    void swap_audio_files(int index1, int index2);
    nlohmann::json queue_info();
    void cplay();

private:
    bool is_playing = false;
    std::chrono::time_point<std::chrono::high_resolution_clock> audio_block_start_time;
    std::vector<std::shared_ptr<AudioFile>> audio_files;
    std::vector<std::weak_ptr<IAudioListener>> listeners;
};

class AudioQueueRwLock
{
public:
    AudioQueueRwLock() : readers(0), writer(false){};

    void lock_write();
    void unlock_write();
    void lock_read();
    void unlock_read();

    AudioQueue &get_queue();

private:
    AudioQueue queue;
    std::mutex mutex;
    std::condition_variable read_condition;
    std::condition_variable write_condition;
    int readers;
    bool writer;
};
