#pragma once

#include <cstdint>
#include <cstring>
#include <chrono>
#include <iostream>

#include "ethernet.hpp"

// Classe que representa uma mensagem genérica para comunicação
class Message {
public:
    // Tamanho máximo da mensagem (em bytes)
    static constexpr size_t MAX_SIZE = 1427; // 1500 - 14 (tamanho do cabeçalho Ethernet) - 59 (tamanho header)
    
    // Construtor: inicializa a mensagem com tamanho zero
    Message() : _size(MAX_SIZE) {}
    
    // Retorna um ponteiro constante para os dados (para leitura)
    const uint8_t* data() const {
        return _data;
    }

    // Retorna um ponteiro para os dados (para escrita)
    uint8_t* data() {
        return _data;
    }
    
    // Retorna o tamanho atual da mensagem
    size_t size() const {
        return _size;
    }
    
    // Define os dados da mensagem copiando do ponteiro 'src'
    void setData(const void* src, size_t size) {
        // Limita o tamanho ao máximo permitido
        _size = (size > MAX_SIZE) ? MAX_SIZE : size;
        // Copia os dados para o buffer interno
        memcpy(_data, src, _size);
    }

    // Define o endereco de origem da mensagem
    void setSrcAddress(const Ethernet::Address src_address) {
        _header.src_address = src_address;
    }

    // Define o endereço de destino da mensagem
    void setDstAddress(const Ethernet::Address dst_address) {
        _header.dst_address = dst_address;
    }

    // Define o tipo da mensagem
    void setType(const Ethernet::Type type) {
        _header.type = type;
    }

    // Define o periodo de transmissao da mensagem
    void setPeriod(const Ethernet::Period period) {
        _header.period = period;
    }

    // Define o timestamp da mensagem
    void setTimestamp(std::chrono::system_clock::time_point tp) {
        // Converte o tempo desde a época para nanosegundos e armazena como uint64_t
        _header.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch()).count();
    }

    // Define o MAC (Message Authentication Code)
    void setMAC(Ethernet::MAC_key mac_key) {
        _header.mac = mac_key;
    }

    // Define o identificador do grupo
    void setGroupID(Ethernet::Quadrant_ID group_id) {
        _header.quadrant_id = group_id;
    }

    // Define a chave MAC do grupo
    void setGroupKey(Ethernet::MAC_key key) {
        _group_key = key;
    }

    // Retorna o endereco de origem da mensagem
    const Ethernet::Address getSrcAddress() const {
        return _header.src_address;
    }

    // Retorna o endereço de destino da mensagem
    const Ethernet::Address getDstAddress() const {
        return _header.dst_address;
    }

    // Retorna o tipo da mensagem
    const Ethernet::Type getType() const {
        return _header.type;
    }
    
    // Retorna o periodo de transmissao da mensagem
    const Ethernet::Period getPeriod() const {
        return _header.period;
    }

    // Retorna o timestamp da mensagem
    std::chrono::system_clock::time_point getTimestamp() const {
        return std::chrono::system_clock::time_point(std::chrono::microseconds(_header.timestamp));
    }

    // Retorna o MAC (Message Authentication Code) da mensagem
    Ethernet::MAC_key getMAC() {
        // Gera MAC se for mensagem de comunicação interna.
        if (_header.src_address.vehicle_id == _header.dst_address.vehicle_id) {
            Ethernet::MAC_key key = {0};
            // Verifica se MAC nao foi gerado anteriormente.
            if (_header.mac == key) {
                _header.mac = generate_mac(_header, _group_key);
            }
        } 
        return _header.mac;
    }

    // Retorna o identificador do grupo
    Ethernet::Quadrant_ID getGroupID() const {
        return _header.quadrant_id;
    }

private:
    // Gera MAC usando o cabeçalho da mensagem (exeto campo mac) e a chave doo grupo.
    Ethernet::MAC_key generate_mac(const Ethernet::ExternalHeader& header, const Ethernet::MAC_key group_key) {
        // Faz XOR dos bytes do cabeçalho (exeto mac) com a chave do grupo.
        Ethernet::MAC_key mac;

        // Copia a chave do grupo como base do MAC
        std::memcpy(mac.data(), group_key.data(), 16);

        // Obtém ponteiro para o início do header
        const uint8_t* data = reinterpret_cast<const uint8_t*>(&header);

        // Calcula o tamanho do cabeçalho sem o campo MAC (últimos 17 bytes: 16 de MAC + 1 de group_id)
        constexpr size_t mac_offset = offsetof(Ethernet::ExternalHeader, mac);
        constexpr size_t mac_field_size = sizeof(Ethernet::MAC_key) + sizeof(Ethernet::Quadrant_ID);
        const size_t size_to_hash = sizeof(Ethernet::ExternalHeader) - mac_field_size;

        // Faz XOR do conteúdo do header com os 16 bytes do MAC base
        for (size_t i = 0; i < size_to_hash; ++i) {
            mac[i % 16] ^= data[i];
        }
        return mac;
    }

private:
    // Cabeçalho da mensagem (suporte para comunicação: externa e interna)
    Ethernet::ExternalHeader _header;
    // Chave MAC do grupo (utilizada no get MAC para comunicação interna)
    Ethernet::MAC_key _group_key;
    // Buffer que armazena os dados da mensagem
    uint8_t _data[MAX_SIZE];
    // Tamanho real da mensagem armazenada
    size_t _size = MAX_SIZE;
};
