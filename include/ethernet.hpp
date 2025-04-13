#pragma once
#include <cstdint>
#include <array>
#include <cstddef>

#include "message.hpp"

#include <iostream>

/**
 * @brief Classe que representa o protocolo Ethernet simplificado para comunicação por broadcast
 * 
 * Fornece as estruturas básicas e constantes necessárias para comunicação em rede
 * usando frames Ethernet no modo broadcast (difusão para todos os nós da rede)
 */
class Ethernet {
public:
    // Define o tipo para endereços MAC (6 bytes)
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

    struct Payload {
        Header header; // Cabeçalho do protocolo 16 bytes
        Message message; // Mensagem a ser transmitida 1470 bytes
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
        Protocol_Number type = 0x88B5;                          // Tipo do protocolo (customizado para o projeto)
        uint8_t payload[MAX_PAYLOAD];                            // Dados transmitidos
        
        /**
         * @brief Retorna o tamanho máximo útil do payload (MTU)
         * @return Tamanho em bytes do payload
         */
        static constexpr size_t mtu() { return MAX_PAYLOAD; }
    } __attribute__((packed));

    bool fillPayload(Ethernet::Frame* frame, const Ethernet::Header* header, const Message* message) {
        size_t payload_size = sizeof(Ethernet::Header) + message->size();
        
        // Verifica se o tamanho do payload não excede o máximo permitido
        if (payload_size > Ethernet::MAX_PAYLOAD) {
            std::cerr << "Payload size exceeds maximum allowed size: " << payload_size << " > " << Ethernet::MAX_PAYLOAD << std::endl;
            return false;
        }
    
        // Ponteiro para o payload em bytes
        uint8_t* payload_bytes = frame->payload;
    
        // Preencher diretamente o buffer com os dados do cabeçalho e mensagem
        std::copy(header->src_address.mac_address.begin(), header->src_address.mac_address.end(), payload_bytes);
        payload_bytes += sizeof(Ethernet::Mac_Address);
    
        *reinterpret_cast<Ethernet::Port*>(payload_bytes) = header->src_address.port;
        payload_bytes += sizeof(Ethernet::Port);
    
        std::copy(header->dst_address.mac_address.begin(), header->dst_address.mac_address.end(), payload_bytes);
        payload_bytes += sizeof(Ethernet::Mac_Address);
    
        *reinterpret_cast<Ethernet::Port*>(payload_bytes) = header->dst_address.port;
        payload_bytes += sizeof(Ethernet::Port);
    
        // Mensagem (dados)
        std::copy(message->data(), message->data() + message->size(), payload_bytes);
    
        return true;
    }
    
    void extractPayload(Ethernet::Frame* frame, Ethernet::Header* header, Message* message) {
        // Ponteiro para o payload em bytes
        uint8_t* payload_bytes = frame->payload;
    
        // Extrair diretamente o conteúdo do payload
        std::copy(payload_bytes, payload_bytes + sizeof(Ethernet::Mac_Address), header->src_address.mac_address.begin());
        payload_bytes += sizeof(Ethernet::Mac_Address);
    
        header->src_address.port = *reinterpret_cast<Ethernet::Port*>(payload_bytes);
        payload_bytes += sizeof(Ethernet::Port);
    
        std::copy(payload_bytes, payload_bytes + sizeof(Ethernet::Mac_Address), header->dst_address.mac_address.begin());
        payload_bytes += sizeof(Ethernet::Mac_Address);
    
        header->dst_address.port = *reinterpret_cast<Ethernet::Port*>(payload_bytes);
        payload_bytes += sizeof(Ethernet::Port);
    
        // A mensagem é o restante dos dados no payload
        size_t message_size = Ethernet::MAX_PAYLOAD - sizeof(Ethernet::Header);
        message->setData(payload_bytes, message_size);
    }
};