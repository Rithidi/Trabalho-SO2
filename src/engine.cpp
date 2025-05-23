#include "../include/engine.hpp"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <thread>
#include <cstdlib>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <pthread.h>
#include <cerrno>

// Static helper function to set thread priority
void Engine::set_thread_priority(std::thread& thread, int policy, int priority) {
    struct sched_param sched_param {};
    sched_param.sched_priority = priority;

    int ret = pthread_setschedparam(thread.native_handle(), policy, &sched_param);
    if (ret != 0) {
        std::cerr << "Failed to set thread priority: " << std::strerror(errno) << std::endl;
    }
}

// Construtor da classe Engine
Engine::Engine(const std::string& interface, Callback callback, bool enable_receive)
    : _interface(interface), _callback(callback), _socket(-1) {
    // Cria um socket raw para capturar pacotes Ethernet
    _socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (_socket < 0) {
        perror("Error creating raw socket");
        exit(EXIT_FAILURE);
    }

    int buffer_size = 4 * 1024 * 1024; // 4 MB
    if (setsockopt(_socket, SOL_SOCKET, SO_RCVBUF, &buffer_size, sizeof(buffer_size)) < 0) {
        perror("Error setting socket receive buffer size");
    }

    // Configura a interface de rede para o socket
    struct ifreq ifr {};
    std::strncpy(ifr.ifr_name, interface.c_str(), IFNAMSIZ - 1);
    if (ioctl(_socket, SIOCGIFINDEX, &ifr) < 0) {
        perror("Error getting interface index");
        close(_socket);
        exit(EXIT_FAILURE);
    }

    // Inicia a thread de recepção, se habilitada
    if (enable_receive) {
        std::thread recv_thread(&Engine::receive_loop, this);
        set_thread_priority(recv_thread, SCHED_FIFO, 50); // High priority for reception thread
        recv_thread.detach();

        // Inicia a thread para processar a fila
        processing_thread = std::thread(&Engine::process_queue, this);
        set_thread_priority(processing_thread, SCHED_FIFO, 40); // Lower priority for processing thread
    }
}

// Destrutor da classe Engine
Engine::~Engine() {
    if (_socket >= 0) {
        close(_socket);
    }

    // Finaliza a thread de processamento
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        stop_processing = true;
    }
    queue_cv.notify_all();
    if (processing_thread.joinable()) {
        processing_thread.join();
    }
}

// Método para enviar um quadro Ethernet
int Engine::send(const void* data, size_t size) {
    struct sockaddr_ll dest_addr {};
    struct ifreq ifr {};
    std::strncpy(ifr.ifr_name, _interface.c_str(), IFNAMSIZ - 1);

    // Obtém o índice da interface de rede
    if (ioctl(_socket, SIOCGIFINDEX, &ifr) < 0) {
        perror("ioctl SIOCGIFINDEX");
        return -1;
    }

    // Configura o endereço de destino para o quadro Ethernet
    dest_addr.sll_family   = AF_PACKET;
    dest_addr.sll_ifindex  = ifr.ifr_ifindex;
    dest_addr.sll_protocol = htons(ETH_P_ALL);
    dest_addr.sll_halen    = ETH_ALEN;

    // Define o endereço MAC de broadcast como destino
    std::memset(dest_addr.sll_addr, 0xFF, 6);

    // Envia o quadro Ethernet
    int sent_bytes = ::sendto(_socket, data, size, 0,
                              (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    if (sent_bytes < 0) {
        perror("Error sending Ethernet frame");
    }
    return sent_bytes;
}

// Método que implementa o loop de recepção de quadros Ethernet
void Engine::receive_loop() {
    constexpr size_t BUFFER_SIZE = 2048;
    char buffer[BUFFER_SIZE];
    const uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    while (true) {
        ssize_t received_bytes = recvfrom(_socket, buffer, BUFFER_SIZE, 0, nullptr, nullptr);
        if (received_bytes > 0) {
            // Verifica se o frame é broadcast
            if (received_bytes >= 14) { // O cabeçalho Ethernet tem pelo menos 14 bytes
                const uint8_t* dest_mac = reinterpret_cast<const uint8_t*>(buffer);
                if (std::memcmp(dest_mac, broadcast_mac, 6) == 0) {
                    // O frame é broadcast, adiciona à fila
                    std::vector<char> data(buffer, buffer + received_bytes);

                    {
                        std::lock_guard<std::mutex> lock(queue_mutex);
                        buffer_queue.emplace(std::move(data), received_bytes);
                    }
                    queue_cv.notify_one();
                }
            }
        }
    }
}

// Método que processa a fila de buffers
void Engine::process_queue() {
    while (true) {
        std::pair<std::vector<char>, size_t> item;

        // Aguarda até que haja um item na fila ou que seja necessário parar
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            queue_cv.wait(lock, [this]() { return !buffer_queue.empty() || stop_processing; });

            if (stop_processing && buffer_queue.empty()) {
                break;
            }

            item = std::move(buffer_queue.front());
            buffer_queue.pop();
        }

        // Processa o item usando o callback
        if (_callback) {
            _callback(item.first.data(), item.second);
        }
    }
}