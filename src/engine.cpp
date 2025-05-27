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
#include <fcntl.h>
#include <semaphore.h>

// Static pointer to the Engine instance for signal handling
static Engine* instance = nullptr;

// Declare a semaphore for signaling
static sem_t receive_semaphore;

// Signal handler for SIGIO
void Engine::sigio_handler(int /*signum*/) {
    // Post to the semaphore to signal the receive loop
    sem_post(&receive_semaphore);
}

// Constructor
Engine::Engine(const std::string& interface, Callback callback, bool enable_receive)
    : _interface(interface), _callback(callback), _socket(-1) {
    instance = this; // Set the global instance pointer

    // Initialize the semaphore
    if (sem_init(&receive_semaphore, 0, 0) != 0) {
        perror("Error initializing semaphore");
        exit(EXIT_FAILURE);
    }

    // Create a raw socket to capture Ethernet packets
    _socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (_socket < 0) {
        perror("Error creating raw socket");
        exit(EXIT_FAILURE);
    }

    int buffer_size = 4 * 1024 * 1024; // 4 MB
    if (setsockopt(_socket, SOL_SOCKET, SO_RCVBUF, &buffer_size, sizeof(buffer_size)) < 0) {
        perror("Error setting socket receive buffer size");
    }

    // Configure the network interface for the socket
    struct ifreq ifr {};
    std::strncpy(ifr.ifr_name, interface.c_str(), IFNAMSIZ - 1);
    if (ioctl(_socket, SIOCGIFINDEX, &ifr) < 0) {
        perror("Error getting interface index");
        close(_socket);
        exit(EXIT_FAILURE);
    }

    // Enable asynchronous I/O and set the owner process for SIGIO
    if (fcntl(_socket, F_SETOWN, getpid()) < 0) {
        perror("Error setting socket owner");
        close(_socket);
        exit(EXIT_FAILURE);
    }
    if (fcntl(_socket, F_SETFL, O_ASYNC | O_NONBLOCK) < 0) {
        perror("Error enabling asynchronous I/O");
        close(_socket);
        exit(EXIT_FAILURE);
    }

    // Set up the SIGIO signal handler
    struct sigaction sa {};
    sa.sa_handler = &Engine::sigio_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGIO, &sa, nullptr) < 0) {
        perror("Error setting up SIGIO handler");
        close(_socket);
        exit(EXIT_FAILURE);
    }

    // Start the receive thread if enabled
    if (enable_receive) {
        std::thread recv_thread(&Engine::receive_loop, this);
        recv_thread.detach();

        // Start the thread to process the queue
        processing_thread = std::thread(&Engine::process_queue, this);
    }
}

// Destructor
Engine::~Engine() {
    if (_socket >= 0) {
        close(_socket);
    }

    // Destroy the semaphore
    sem_destroy(&receive_semaphore);

    // Finalize the processing thread
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        stop_processing = true;
    }
    queue_cv.notify_all();
    if (processing_thread.joinable()) {
        processing_thread.join();
    }
}

// Method implementing the Ethernet frame reception loop
void Engine::receive_loop() {
    constexpr size_t BUFFER_SIZE = 2048;
    char buffer[BUFFER_SIZE];
    const uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    while (true) {
        // Wait for the semaphore to be posted
        sem_wait(&receive_semaphore);

        // Process incoming frames
        while (true) {
            ssize_t received_bytes = recvfrom(_socket, buffer, BUFFER_SIZE, 0, nullptr, nullptr);

            if (received_bytes > 0) {
                // Check if the frame is broadcast
                if (received_bytes >= 14) { // Ethernet header is at least 14 bytes
                    const uint8_t* dest_mac = reinterpret_cast<const uint8_t*>(buffer);
                    if (std::memcmp(dest_mac, broadcast_mac, 6) == 0) {
                        // The frame is broadcast, add it to the queue
                        std::vector<char> data(buffer, buffer + received_bytes);

                        {
                            std::lock_guard<std::mutex> lock(queue_mutex);
                            buffer_queue.emplace(std::move(data), received_bytes);
                        }
                        queue_cv.notify_one();
                    }
                }
            } else if (received_bytes < 0) {
                // Check if there are no more frames to read
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break; // No more frames available, break out of the inner loop
                } else {
                    //perror("Error receiving Ethernet frame");
                }
            }
        }
    }
}

// Method to process the buffer queue
void Engine::process_queue() {
    while (true) {
        std::pair<std::vector<char>, size_t> item;

        // Wait until there is an item in the queue or processing needs to stop
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

// Method to send an Ethernet frame
int Engine::send(const void* data, size_t size) {
    struct sockaddr_ll dest_addr {};
    struct ifreq ifr {};
    std::strncpy(ifr.ifr_name, _interface.c_str(), IFNAMSIZ - 1);

    // Get the network interface index
    if (ioctl(_socket, SIOCGIFINDEX, &ifr) < 0) {
        perror("ioctl SIOCGIFINDEX");
        return -1;
    }

    // Configure the destination address for the Ethernet frame
    dest_addr.sll_family   = AF_PACKET;
    dest_addr.sll_ifindex  = ifr.ifr_ifindex;
    dest_addr.sll_protocol = htons(ETH_P_ALL);
    dest_addr.sll_halen    = ETH_ALEN;

    // Set the broadcast MAC address as the destination
    std::memset(dest_addr.sll_addr, 0xFF, 6);

    // Temporarily set the socket to blocking mode
    int flags = fcntl(_socket, F_GETFL, 0);
    fcntl(_socket, F_SETFL, flags & ~O_NONBLOCK);

    // Perform the send operation
    int sent_bytes = ::sendto(_socket, data, size, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));

    // Restore the socket to non-blocking mode
    fcntl(_socket, F_SETFL, flags | O_NONBLOCK);

    if (sent_bytes < 0) {
        perror("Error sending Ethernet frame");
        return -1;
    }
    return sent_bytes;
}
