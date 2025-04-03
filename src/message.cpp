#include "../include/message.hpp"
#include <cstring>  // Para memcpy

// Construtor padrão que inicializa o tamanho da mensagem como 0.
Message::Message() {
    _size = 0;
}

// Retorna um ponteiro para os dados da mensagem.
uint8_t* Message::data() {
    return _data;
}

// Retorna o tamanho atual da mensagem.
size_t Message::size() const {
    return _size;
}

// Define os dados da mensagem copiando de uma fonte (src) até o tamanho especificado.
// Se o tamanho exceder o limite máximo, ele será truncado para MAX_SIZE.
void Message::setData(const void* src, size_t size) {
    _size = (size > MAX_SIZE) ? MAX_SIZE : size;
    memcpy(_data, src, _size);
}

