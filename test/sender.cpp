#include "nic.hpp"
#include <iostream>
#include <cstring>
#include <arpa/inet.h>

int main() {
    // Inicializa a NIC com o mecanismo de rede (Engine) e a interface de rede "eth0"
    NIC<Engine> nic("enp6s0");

    // Configura o endereço MAC da interface (opcional, se necessário)
    NIC<Engine>::Mac_Address mac_address = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    nic.set_address(mac_address);

    // Tamanho do payload
    constexpr size_t PAYLOAD_SIZE = 64;

    // Número de mensagens a serem enviadas
    constexpr int NUM_MESSAGES = 1;

    for (int i = 0; i < NUM_MESSAGES; ++i) {
        // Aloca um buffer para o frame Ethernet
        auto* buffer = nic.alloc({}, 0x0800, PAYLOAD_SIZE);  // Protocolo IPv4 (0x0800)
        if (!buffer) {
            std::cerr << "Erro ao alocar o buffer!" << std::endl;
            return -1;
        }

        // Preenche o payload com a mensagem
        const char* msg = "Hello Ethernet Broadcast!";
        std::memcpy(buffer->frame.payload, msg, std::min(strlen(msg), PAYLOAD_SIZE));
        buffer->size = std::min(strlen(msg), PAYLOAD_SIZE);

        // Configura o endereço MAC de destino (broadcast)
        buffer->frame.dst = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

        // Configura o endereço MAC de origem
        buffer->frame.src = mac_address;

        // Configura o tipo do protocolo (IPv4)
        buffer->frame.type = htons(0x88B5);

        // Envia o frame
        int result = nic.send(buffer);

        if (result > 0) {
            std::cout << "Frame " << i + 1 << " enviado com sucesso!" << std::endl;
        } else {
            std::cerr << "Erro ao enviar o frame " << i + 1 << "!" << std::endl;
        }

        // Libera o buffer alocado
        delete buffer;
        //std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::cout << "Enviados " << NUM_MESSAGES << " frames de broadcast." << std::endl;

    return 0;
}
