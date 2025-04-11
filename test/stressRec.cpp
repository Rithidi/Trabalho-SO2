#include "engine.hpp"
#include <iostream>
#include <csignal>
#include <atomic>

// Contador atômico para o total de pacotes recebidos
std::atomic<size_t> total_packets(0);

// Callback que incrementa o contador e exibe o total de pacotes recebidos
void count_and_display_frame(const void* /*data*/, size_t /*size*/) {
    // Incrementa o contador
    size_t current_count = ++total_packets;

    // Exibe o total de pacotes recebidos
    if (current_count % 1000 == 0) { // Exibe a cada 1000 pacotes
        std::cout << "\rTotal packets received: " << current_count << std::flush;
    }
    //std::cout << "Total packets received: " << current_count << "\n";
}

int main() {
    std::string interface = "enp7s0"; // Altere para a interface desejada

    // Passa o callback para a Engine
    Engine engine(interface, count_and_display_frame, true);

    std::cout << "Listening for Ethernet frames on " << interface << "...\n";
    std::cout << "Press Ctrl+C to exit.\n";

    // Captura Ctrl+C para sair do programa
    std::signal(SIGINT, [](int) {
        std::cout << "\nExiting.\n";
        std::exit(0);
    });

    // Bloqueia indefinidamente
    while (true) {
        pause();
    }

    return 0;
}