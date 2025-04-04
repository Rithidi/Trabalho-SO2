#pragma once
#include <cstdint>
#include <cstring>

// Classe que representa uma mensagem genérica para comunicação
class Message {
public:
    // Tamanho máximo da mensagem (em bytes)
    static constexpr size_t MAX_SIZE = 1500;
    
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

private:
    // Buffer que armazena os dados da mensagem
    uint8_t _data[MAX_SIZE];

    // Tamanho real da mensagem armazenada
    size_t _size;
};
