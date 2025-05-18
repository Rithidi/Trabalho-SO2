#pragma once
#include <cstdint>
#include <cstring>
#include "ethernet.hpp"

// Classe que representa uma mensagem genérica para comunicação
class Message {
public:
    // Tamanho máximo da mensagem (em bytes)
    static constexpr size_t MAX_SIZE = 1452; // 1500 - 14 (tamanho do cabeçalho Ethernet) - 34 (tamanho header)
    
    // Construtor: inicializa a mensagem com tamanho zero
    Message() : _size(0) {}
    
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

private:
    // Cabeçalho da mensagem (endereços de origem e destino, tipo e período)
    Ethernet::Header _header;
    // Buffer que armazena os dados da mensagem
    uint8_t _data[MAX_SIZE];
    // Tamanho real da mensagem armazenada
    size_t _size = MAX_SIZE;
};
