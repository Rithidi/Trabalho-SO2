#pragma once

#include <cstdint>
#include <array>
#include <cstddef>
#include <pthread.h>
#include <iostream>
#include <cstring>
#include <tuple>

// Classe responsável por montar e desmontar os frames de comunicação.
class Ethernet {
public:
    using Protocol_Number = uint16_t;           // (2 bytes)
    using Mac_Address = std::array<uint8_t, 6>; // (6 bytes)
    using Thread_ID = pthread_t;                // (8 bytes)
    using Type = uint32_t;                      // (4 bytes)
    using Period = uint16_t;                    // (2 bytes)
    using Timestamp = uint64_t;                 // (8 bytes)
    using MAC_key = std::array<uint8_t, 16>;    // (16 bytes)
    using Group_ID = uint8_t;                   // (1 byte)

    // Tipos de dados utilizados pelo Time Synchronization Manager (PTP - IEEE 1588).
    Ethernet::Type constexpr static TYPE_PTP_SYNC = 0x0;        // (4 bytes) Tipo de dado PTP Sync
    Ethernet::Type constexpr static TYPE_PTP_DELAY_REQ = 0x01;  // (4 bytes) Tipo de dado PTP Delay Request
    Ethernet::Type constexpr static TYPE_PTP_DELAY_RESP = 0x09; // (4 bytes) Tipo de dado PTP Delay Response

    // Tipos de dados utilizados para comunicacao entre RSU e veiculos.
    Ethernet::Type constexpr static TYPE_RSU_JOIN_REQ = 0x0A;   // (4 bytes)
    Ethernet::Type constexpr static TYPE_RSU_JOIN_RESP = 0x0B;  // (4 bytes)

    // Tipo de dado enviado pelos componentes que fornecem a posicao dos veiculo.
    Ethernet::Type constexpr static TYPE_POSITION_DATA = 0x0D;

    // Estrturua de dados para envio da Localizacao dos veiculos.
    struct Position {
        int x;
        int y;
    };

    // Estrutura de dados para envio do Quadrante dos RSUs.
    struct Quadrant {
        int x_min;
        int x_max;
        int y_min;
        int y_max;
    };

    // Estrutura para armazenar o endereço dos componentes do veículo.
    struct Address { // (14 bytes)
        Mac_Address vehicle_id = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // Endereço MAC (identificador do carro) (6 bytes) // // Inicializa com valor inexistente
        Thread_ID component_id = (pthread_t)0; // ID da thread (identificador do componente) (8 bytes) // Inicializa com valor inexistente
        // operador < (já implementado)
        bool operator<(const Address& other) const {
            return std::tie(vehicle_id, component_id) < std::tie(other.vehicle_id, other.component_id);
        }

        // operador ==
        bool operator==(const Address& other) const {
            return std::tie(vehicle_id, component_id) == std::tie(other.vehicle_id, other.component_id);
        }

        // operador >
        bool operator>(const Address& other) const {
            return std::tie(vehicle_id, component_id) > std::tie(other.vehicle_id, other.component_id);
        }
    } __attribute__((packed));

    // Estrutura para armazenar o cabeçalho da aplicação.
    struct Header { // (59 bytes)
        Address src_address; // Endereço de origem (14 bytes)
        Address dst_address; // Endereço de destino (14 bytes)
        Type type;           // Tipo do dado (4 bytes)
        Period period = 0;   // Período de transmissão em milissegundos (max 65s) (2 bytes)
        Timestamp timestamp; // Timestamp do envio da mensagem (8 bytes)
        MAC_key mac;         // Message Authentication Code (16 bytes)
        Group_ID group_id;   // Identificador do grupo (1 byte)
    } __attribute__((packed));

    // Estrutura para armazenar o payload da aplicação.
    struct Payload {
        Header header; // Cabeçalho da aplicacao 59 bytes
        uint8_t data[1427]; // Mensagem a ser transmitida 1427 bytes
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