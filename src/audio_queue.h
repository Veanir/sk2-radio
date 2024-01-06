#pragma once

#include "audio_file.h"
#include <chrono>
#include <functional>
#include <memory>
#include <vector>
#include <mutex>
#include <condition_variable>

class AudioQueue
{
public:
    AudioQueue();

    void push(std::shared_ptr<AudioFile> file);
    void subscribe(std::function<void(std::shared_ptr<AudioBlock>)> callback);
    void update();

private:
    void update_listeners(std::shared_ptr<AudioBlock> block);

    std::chrono::time_point<std::chrono::high_resolution_clock> audio_block_start_time;
    std::vector<std::shared_ptr<AudioFile>> audio_files;
    std::vector<std::function<void(std::shared_ptr<AudioBlock>)>> callbacks;
};

class AudioQueueRwLock
{
public:
    AudioQueueRwLock() : readers(0), writer(false){};
    ~AudioQueueRwLock();

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
