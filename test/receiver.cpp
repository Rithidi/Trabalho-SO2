#include "Engine.hpp"
#include <iostream>
#include <iomanip>
#include <csignal>
#include <cstdint>
#include <cstring>

void print_frame(const void* data, size_t size) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    std::cout << "[Frame received] Size: " << size << " bytes\n";
    for (size_t i = 0; i < size; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(bytes[i]) << " ";
        if ((i + 1) % 16 == 0)
            std::cout << "\n";
    }
    std::cout << std::dec << "\n\n";
}

int main() {
    std::string interface = "enp7s0"; // Change this

    // Pass the callback to Engine
    Engine engine(interface, print_frame, true);

    std::cout << "Listening for Ethernet frames on " << interface << "...\n";
    std::cout << "Press Ctrl+C to exit.\n";

    // Block forever
    std::signal(SIGINT, [](int) {
        std::cout << "\nExiting.\n";
        std::exit(0);
    });

    while (true) {
        pause();
    }

    return 0;
}

