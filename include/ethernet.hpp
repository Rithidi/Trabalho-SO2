#pragma once
#include <cstdint>
#include <array>
#include <cstddef>
#include <pthread.h>

#include <iostream>
#include <cstring>


 // Classe responsável por montar e desmontar os frames de comunicação.
class Ethernet {
public:
    using Protocol_Number = uint16_t; // (2 bytes)
    using Mac_Address = std::array<uint8_t, 6>; // (6 bytes)
    using Thread_ID = pthread_t; // (8 bytes)
    using Type = uint32_t; // (4 bytes)
    using Period = uint16_t; // (2 bytes)

    // Estrutura para armazenar o endereço dos componentes do veículo.
    struct Address { // (14 bytes)
        Mac_Address vehicle_id = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // Endereço MAC (identificador do carro) (6 bytes) // // Inicializa com valor inexistente
        Thread_ID component_id = (pthread_t)0; // ID da thread (identificador do componente) (8 bytes) // Inicializa com valor inexistente
    } __attribute__((packed));

    // Estrutura para armazenar o cabeçalho da aplicação.
    struct Header { // (34 bytes)
        Address src_address; // Endereço de origem (14 bytes)
        Address dst_address; // Endereço de destino (14 bytes)
        Type type; // Tipo do dado (4 bytes)
        Period period; // Período de transmissão em milissegundos (max 65s) (2 bytes)
    } __attribute__((packed));

    // Estrutura para armazenar o payload da aplicação.
    struct Payload {
        Header header; // Cabeçalho da aplicacao 34 bytes
        uint8_t data[1452]; // Mensagem a ser transmitida 1452 bytes
    } __attribute__((packed));

    // Tamanho do cabeçalho Ethernet em bytes (destino + origem + tipo)
    static constexpr size_t HEADER_SIZE = 14; // 6(dst) + 6(src) + 2(type)
    
    // Tamanho máximo do payload (MTU padrão - tamanho do cabeçalho)
    static constexpr size_t MAX_PAYLOAD = 1500 - HEADER_SIZE;

    // Estrutura do frame Ethernet.
    struct Frame {
        Mac_Address dst = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // Endereço MAC de destino (sempre broadcast)
        Mac_Address src;                                        // Endereço MAC de origem
        Protocol_Number type = 0x88B5;                          // Tipo do protocolo (customizado para o projeto)
        uint8_t payload[MAX_PAYLOAD];                            // Dados transmitidos
        
        // Retorna o tamanho máximo do payload.
        static constexpr size_t mtu() { return MAX_PAYLOAD; }
    } __attribute__((packed));

    // Metodo para preencher o payload do frame.
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

    // Metodo para extrair o payload do frame.
    void extractPayload(Ethernet::Frame* frame, Ethernet::Payload* payload) {
        // Copia o conteúdo do vetor de bytes frame->payload para a estrutura Payload
        std::memcpy(payload, frame->payload, sizeof(Ethernet::Payload));
    }
};