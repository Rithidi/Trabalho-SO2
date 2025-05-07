#pragma once

#include <functional>
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>

class Engine {
public:
    using Callback = std::function<void(const void*, size_t)>;

    // Construtor com callback opcional
    Engine(const std::string& interface, Callback callback = nullptr, bool enable_receive = false);

    ~Engine();

    int send(const void* data, size_t size);

    // Métodos para acesso controlado às variáveis privadas
    int get_socket() const { return _socket; }
    std::mutex& get_queue_mutex() { return queue_mutex; }
    std::queue<std::pair<std::vector<char>, size_t>>& get_buffer_queue() { return buffer_queue; }
    std::condition_variable& get_queue_cv() { return queue_cv; }

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
};

