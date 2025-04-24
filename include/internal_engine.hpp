#ifndef INTERNAL_ENGINE_HPP
#define INTERNAL_ENGINE_HPP

#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>

// Define a callback type
using Callback = std::function<void(const void*, size_t)>;

class InternalEngine {
public:
    // Constructor
    InternalEngine(Callback callback);

    // Destructor
    ~InternalEngine();

    // Method to enqueue data for processing
    void send(const void* data, size_t size);

private:
    // Method to process the queue
    void process_queue();

    // Callback function for processing data
    Callback _callback;

    // Thread for processing the queue
    std::thread processing_thread;

    // Queue to hold data(memoria interna compartilhada)
    std::queue<std::pair<std::vector<char>, size_t>> buffer_queue;

    // Mutex and condition variable for thread synchronization
    std::mutex queue_mutex;
    std::condition_variable queue_cv;

    // Flag to stop the processing thread
    bool stop_processing;
};

#endif // INTERNAL_ENGINE_HPP