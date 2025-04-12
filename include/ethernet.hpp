#pragma once
#include <cstdint>
#include <array>
#include <cstddef>

/**
 * @brief Classe que representa o protocolo Ethernet simplificado para comunicação por broadcast
 * 
 * Fornece as estruturas básicas e constantes necessárias para comunicação em rede
 * usando frames Ethernet no modo broadcast (difusão para todos os nós da rede)
 */
class Ethernet {
public:
    // Define o tipo para endereços MAC (6 bytes)
    //using Address = std::array<uint8_t, 6>;

    // Define o tipo para números de protocolo (16 bits)
    using Protocol_Number = uint16_t;
    using Mac_Address = std::array<uint8_t, 6>;
    typedef unsigned short Port;

    struct Address {
        Mac_Address mac_address; // Endereço MAC
        Port port; // Porta de origem
    };
  
    struct Header {
        Address src_address; // Endereço de origem
        Address dst_address; // Endereço de destino
    };
  
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
        Mac_Address dst = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // Endereço MAC de destino (sempre broadcast)
        Mac_Address src;                                        // Endereço MAC de origem
        Protocol_Number type = 0x88B5;                     // Tipo do protocolo (customizado para o projeto)
        
        // Cabeçalho do protocolo
        Header header;
        //

        uint8_t payload[MAX_PAYLOAD];                      // Dados transmitidos
        
        /**
         * @brief Retorna o tamanho máximo útil do payload (MTU)
         * @return Tamanho em bytes do payload
         */
        static constexpr size_t mtu() { return MAX_PAYLOAD; }
    } __attribute__((packed)); // Garante que o compilador não adicione padding entre os campos
};