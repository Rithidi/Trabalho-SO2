#include "Engine.hpp"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <net/ethernet.h>
#include <netinet/if_ether.h>
#include <linux/if_packet.h>

int main() {
    const std::string interface = "enp7s0"; // Substitua pela interface correta
    Engine engine(interface, nullptr);  // Sem callback, pois apenas envia

    // Criando um frame Ethernet de broadcast
    constexpr size_t FRAME_SIZE = 64;
    unsigned char frame[FRAME_SIZE] = {0};

    // Configurar endereço de destino (broadcast)
    std::memset(frame, 0xFF, 6); // MAC de broadcast FF:FF:FF:FF:FF:FF
    std::memset(frame + 6, 0x11, 6); // MAC de origem (fictício)
    frame[12] = 0x08;  // Tipo Ethernet (0x0800 = IPv4)
    frame[13] = 0x00;

    // Dados do frame
    const char* msg = "Hello Ethernet Broadcast!";
    std::memcpy(frame + 14, msg, std::min(strlen(msg), FRAME_SIZE - 14));

    std::cout << "Enviando frame de broadcast...\n";
    engine.send(frame, FRAME_SIZE);
    std::cout << "Frame enviado!\n";

    return 0;
}
