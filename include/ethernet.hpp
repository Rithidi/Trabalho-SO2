#pragma once
#include <cstdint>
#include <array>
#include <cstddef>

#include "message.hpp"

#include <iostream>

#include <pthread.h>

/**
 * @brief Classe que representa o protocolo Ethernet simplificado para comunicação por broadcast
 * 
 * Fornece as estruturas básicas e constantes necessárias para comunicação em rede
 * usando frames Ethernet no modo broadcast (difusão para todos os nós da rede)
 */
class Ethernet {
public:
    using Protocol_Number = uint16_t; // (2 bytes)
    using Mac_Address = std::array<uint8_t, 6>; // (6 bytes)
    using Thread_ID = pthread_t; // (8 bytes)
    typedef unsigned short Port; // (2 bytes)

    struct Address { // (16 bytes)
        Mac_Address vehicle_id = {0, 0, 0, 0, 0, 0}; // Endereço MAC (identificador do carro) (6 bytes) // // Inicializa com valor inexistente
        Thread_ID component_id = (pthread_t)0; // ID da thread (identificador do componente) (8 bytes) // Inicializa com valor inexistente
        Port port; // Porta (2 bytes)
    } __attribute__((packed));

    struct Header { // (32 bytes)
        Address src_address; // Endereço de origem (16 bytes)
        Address dst_address; // Endereço de destino (16 bytes)
    } __attribute__((packed));

    struct Payload {
        Header header; // Cabeçalho do protocolo 32 bytes
        uint8_t data[1454]; // Mensagem a ser transmitida 1454 bytes
    } __attribute__((packed));

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
        Protocol_Number type = 0x88B5;                          // Tipo do protocolo (customizado para o projeto)
        uint8_t payload[MAX_PAYLOAD];                            // Dados transmitidos
        
        /**
         * @brief Retorna o tamanho máximo útil do payload (MTU)
         * @return Tamanho em bytes do payload
         */
        static constexpr size_t mtu() { return MAX_PAYLOAD; }
    } __attribute__((packed));


    bool fillPayload(Ethernet::Frame* frame, const Payload* payload) {
        size_t payload_size = sizeof(Ethernet::Payload);
        // Verifica se o tamanho do payload não excede o máximo permitido
        if (payload_size > Ethernet::MAX_PAYLOAD) {
            std::cerr << "Payload size exceeds maximum allowed size: " << payload_size << " > " << Ethernet::MAX_PAYLOAD << std::endl;
            return false;
        }
        // Copiar a estrutura Payload para o vetor de bytes frame->payload
        std::memcpy(frame->payload, payload, payload_size);
        return true;
    }
    
    void extractPayload(Ethernet::Frame* frame, Ethernet::Payload* payload) {
        // Copia o conteúdo do vetor de bytes frame->payload para a estrutura Payload
        std::memcpy(payload, frame->payload, sizeof(Ethernet::Payload));
    }
};