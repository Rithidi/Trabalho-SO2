#pragma once
#include <cstdint>
#include <array>


class Ethernet {
public:
    // Tamanho do cabeçalho Ethernet em bytes (destino + origem + tipo)
    static constexpr size_t HEADER_SIZE = 14; // 6(dst) + 6(src) + 2(type)
    
    // Tamanho máximo do payload (MTU padrão - tamanho do cabeçalho)
    static constexpr size_t MAX_PAYLOAD = 1500 - HEADER_SIZE;
    

    //Estrutura que representa um frame Ethernet
    struct Frame {
        std::array<uint8_t, 6> dst = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // Endereço MAC de destino (sempre broadcast)
        std::array<uint8_t, 6> src;                                        // Endereço MAC de origem
        uint16_t type = 0x88B5;                                            // Tipo do protocolo (customizado para o projeto)
        uint8_t payload[MAX_PAYLOAD];                                      // Dados transmitidos
        

        //Retorna o tamanho máximo (em bytes) útil do payload (MTU)
        static constexpr size_t mtu() { return MAX_PAYLOAD; }
    } __attribute__((packed)); // Garante que o compilador não adicione padding entre os campos
};