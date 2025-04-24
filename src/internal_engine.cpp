#include "../include/internal_engine.hpp"
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>

// Constructor for InternalEngine
InternalEngine::InternalEngine(Callback callback)
    : _callback(callback), stop_processing(false) {
    // Start the processing thread
    processing_thread = std::thread(&InternalEngine::process_queue, this);
}

// Destructor for InternalEngine
InternalEngine::~InternalEngine() {
    // Signal the processing thread to stop
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        stop_processing = true;
    }
    queue_cv.notify_all();

    // Join the processing thread
    if (processing_thread.joinable()) {
        processing_thread.join();
    }
}

// Method to add data to the queue
void InternalEngine::send(const void* data, size_t size) {
    std::vector<char> buffer(static_cast<const char*>(data), static_cast<const char*>(data) + size);

    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        buffer_queue.emplace(std::move(buffer), size);
    }
    queue_cv.notify_one();
}

// Method to process the queue
void InternalEngine::process_queue() {
    while (true) {
        std::pair<std::vector<char>, size_t> item;

        // Wait until there is data in the queue or stop_processing is true
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            queue_cv.wait(lock, [this]() { return !buffer_queue.empty() || stop_processing; });

            if (stop_processing && buffer_queue.empty()) {
                break;
            }

            item = std::move(buffer_queue.front());
            buffer_queue.pop();
        }

        // Process the item using the callback
        if (_callback) {
            _callback(item.first.data(), item.second);
        }
    }
}