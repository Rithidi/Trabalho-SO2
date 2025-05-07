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
#include <csignal>
#include <fcntl.h> // Inclui fcntl.h para F_SETFL, O_ASYNC, etc.

// Ponteiro estático para a instância da classe Engine
static Engine* engine_instance = nullptr;

// Função estática para manipular SIGIO
static void sigio_handler(int signo) {
    if (signo == SIGIO && engine_instance) {
        constexpr size_t BUFFER_SIZE = 2048;
        char buffer[BUFFER_SIZE];
        const uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

        // Recebe os dados do socket
        ssize_t received_bytes = recvfrom(engine_instance->_socket, buffer, BUFFER_SIZE, 0, nullptr, nullptr);
        if (received_bytes > 0) {
            // Verifica se o frame é broadcast
            if (received_bytes >= 14) { // O cabeçalho Ethernet tem pelo menos 14 bytes
                const uint8_t* dest_mac = reinterpret_cast<const uint8_t*>(buffer);
                if (std::memcmp(dest_mac, broadcast_mac, 6) == 0) {
                    // O frame é broadcast, adiciona à fila
                    std::vector<char> data(buffer, buffer + received_bytes);

                    {
                        std::lock_guard<std::mutex> lock(engine_instance->queue_mutex);
                        engine_instance->buffer_queue.emplace(std::move(data), received_bytes);
                    }
                    engine_instance->queue_cv.notify_one();
                }
            }
        }
    }
}

// Construtor da classe Engine
Engine::Engine(const std::string& interface, Callback callback, bool enable_receive)
    : _interface(interface), _callback(callback), _socket(-1) {
    // Define a instância estática
    engine_instance = this;

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

    // Configura o socket para modo assíncrono
    if (fcntl(_socket, F_SETFL, O_ASYNC | O_NONBLOCK) < 0) {
        perror("Erro ao configurar modo assíncrono no socket");
        close(_socket);
        exit(EXIT_FAILURE);
    }

    // Define o processo atual como destinatário do SIGIO
    if (fcntl(_socket, F_SETOWN, getpid()) < 0) {
        perror("Erro ao configurar proprietário do SIGIO");
        close(_socket);
        exit(EXIT_FAILURE);
    }

    // Registra o manipulador para SIGIO
    struct sigaction sa {};
    sa.sa_handler = sigio_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGIO, &sa, nullptr) < 0) {
        perror("Erro ao registrar manipulador para SIGIO");
        close(_socket);
        exit(EXIT_FAILURE);
    }

    // Inicia a thread para processar a fila, se habilitada
    if (enable_receive) {
        processing_thread = std::thread(&Engine::process_queue, this);
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
