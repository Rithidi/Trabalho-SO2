#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <string>
#include <functional>
#include <thread>
#include <atomic>

using Callback = std::function<void(const void*, size_t)>;

class Engine {
public:
    Engine(const std::string& interface, Callback callback = nullptr, bool enable_receive = true);
    ~Engine();

    int send(const void* data, size_t size);

private:
    void receive_loop();

    std::string _interface;
    Callback _callback;
    int _socket;
    std::atomic<bool> _running = false;
    std::thread _recv_thread;
};

#endif // ENGINE_HPP

