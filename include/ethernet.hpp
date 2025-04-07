#pragma once
#include <cstdint>
#include <array>

/**
 * @brief Classe que representa o protocolo Ethernet simplificado para comunicação por broadcast
 * 
 * Fornece as estruturas básicas e constantes necessárias para comunicação em rede
 * usando frames Ethernet no modo broadcast (difusão para todos os nós da rede)
 */
class Ethernet {
public:
    // Tamanho do cabeçalho Ethernet em bytes (destino + origem + tipo)
    static constexpr size_t HEADER_SIZE = 14; // 6(dst) + 6(src) + 2(type)
    
    // Tamanho máximo do payload (MTU padrão - tamanho do cabeçalho)
    static constexpr size_t MAX_PAYLOAD = 1500 - HEADER_SIZE;
    
    /**
     * @brief Estrutura que representa um frame Ethernet
     * 
     * Organizada exatamente como o frame será transmitido na rede
     * com alinhamento de bytes compactado (sem padding)
     */
    struct Frame {
        std::array<uint8_t, 6> dst = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // Endereço MAC de destino (sempre broadcast)
        std::array<uint8_t, 6> src;                                        // Endereço MAC de origem
        uint16_t type = 0x88B5;                                            // Tipo do protocolo (customizado para o projeto)
        uint8_t payload[MAX_PAYLOAD];                                      // Dados transmitidos
        
        /**
         * @brief Retorna o tamanho máximo útil do payload (MTU)
         * @return Tamanho em bytes do payload
         */
        static constexpr size_t mtu() { return MAX_PAYLOAD; }
    } __attribute__((packed)); // Garante que o compilador não adicione padding entre os campos
};