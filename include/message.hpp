#pragma once
#include <cstdint>

// Classe Message representa uma mensagem com tamanho máximo definido (MTU Ethernet).
class Message {
    public:
        // Tamanho máximo da mensagem (1500 bytes, típico do MTU Ethernet).
        static constexpr size_t MAX_SIZE = 1500;

        // Construtor padrão que inicializa o tamanho da mensagem como 0.
        Message();

        // Retorna um ponteiro para os dados da mensagem.
        uint8_t* data();

        // Retorna o tamanho atual da mensagem.
        size_t size() const;
        
        // Define os dados da mensagem copiando de uma fonte (src) até o tamanho especificado.
        // Se o tamanho exceder o limite máximo, ele será truncado para MAX_SIZE.
        void setData(const void* src, size_t size);

    private:
        // Buffer para armazenar os dados da mensagem, com tamanho máximo definido.
        uint8_t _data[MAX_SIZE];

        // Tamanho atual da mensagem armazenada no buffer.
        size_t _size;
};
