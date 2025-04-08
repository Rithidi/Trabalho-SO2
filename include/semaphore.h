#pragma once

#include <mutex>
#include <condition_variable>

class Semaphore {
public:
    explicit Semaphore(int count = 0)
        : _count(count) {}

    void p() {
        std::unique_lock<std::mutex> lock(_mutex);
        _cv.wait(lock, [&]() { return _count > 0; });
        _count--;
    }

    void v() {
        std::unique_lock<std::mutex> lock(_mutex);
        _count++;
        _cv.notify_one();
    }

private:
    std::mutex _mutex;
    std::condition_variable _cv;
    int _count;
};
