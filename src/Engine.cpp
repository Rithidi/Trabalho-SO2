#include "engine.hpp"
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

Engine::Engine(const std::string& interface, Callback callback, bool enable_receive)
    : _interface(interface), _callback(callback)
{
    _socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (_socket < 0) {
        perror("Error creating raw socket");
        exit(EXIT_FAILURE);
    }

    struct ifreq ifr {};
    std::strncpy(ifr.ifr_name, interface.c_str(), IFNAMSIZ - 1);
    if (ioctl(_socket, SIOCGIFINDEX, &ifr) < 0) {
        perror("Error getting interface index");
        close(_socket);
        exit(EXIT_FAILURE);
    }

    std::thread recv_thread(&Engine::receive_loop, this);
    recv_thread.detach();
}

Engine::~Engine() {
    if (_socket >= 0) {
        close(_socket);
    }
}

int Engine::send(const void* data, size_t size) {
    struct sockaddr_ll dest_addr {};
    struct ifreq ifr {};
    std::strncpy(ifr.ifr_name, _interface.c_str(), IFNAMSIZ - 1);

    // Get interface index
    if (ioctl(_socket, SIOCGIFINDEX, &ifr) < 0) {
        perror("ioctl SIOCGIFINDEX");
        return -1;
    }

    dest_addr.sll_family   = AF_PACKET;
    dest_addr.sll_ifindex  = ifr.ifr_ifindex;
    dest_addr.sll_protocol = htons(ETH_P_ALL);
    dest_addr.sll_halen    = ETH_ALEN;

    // Set broadcast MAC address as destination
    std::memset(dest_addr.sll_addr, 0xFF, 6);

    // Send the frame
    int sent_bytes = ::sendto(_socket, data, size, 0,
                              (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    if (sent_bytes < 0) {
        perror("Error sending Ethernet frame");
    }
    return sent_bytes;
}

void Engine::receive_loop() {
    constexpr size_t BUFFER_SIZE = 2048;
    char buffer[BUFFER_SIZE];
    const uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    while (true) {
        ssize_t received_bytes = recvfrom(_socket, buffer, BUFFER_SIZE, 0, nullptr, nullptr);
        if (received_bytes > 0 && _callback) {
            if (received_bytes >= 14) {
                const uint8_t* dest_mac = reinterpret_cast<const uint8_t*>(buffer);
                if (std::memcmp(dest_mac, broadcast_mac, 6) == 0) {
                    _callback(buffer, received_bytes);
                }
            }
        } else if (received_bytes < 0) {
            perror("Error receiving Ethernet frame");
        }
    }
}
