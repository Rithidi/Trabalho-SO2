#pragma once

#include <functional>
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include <semaphore.h>

class Engine {
public:
    using Callback = std::function<void(const void*, size_t)>;

    // Construtor com callback opcional
    Engine(const std::string& interface, Callback callback = nullptr, bool enable_receive = false);

    ~Engine();

    int send(const void* data, size_t size);
    static void set_thread_priority(std::thread& thread, int policy, int priority);

private:
    std::string _interface;
    Callback _callback;
    int _socket;

    // Fila para armazenar os buffers recebidos
    std::queue<std::pair<std::vector<char>, size_t>> buffer_queue;
    std::mutex queue_mutex;
    std::condition_variable queue_cv;

    // Thread para processar a fila
    std::thread processing_thread;
    bool stop_processing = false;

    void receive_loop();
    void process_queue();

    // SIGIO signal handler
    static void sigio_handler(int signum);
};