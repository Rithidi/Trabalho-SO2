#include "../include/message.hpp"
#include <cstring>


 // Construtor que aceita dados brutos
Message::Message(const unsigned char* bytes, unsigned int size) {
    set_data(bytes, size);
}

// Define novos dados
void Message::set_data(const unsigned char* bytes, unsigned int size) {
    if (bytes && size > 0) {
        _data.resize(size);               // Redimensiona o vetor
        memcpy(_data.data(), bytes, size); // Copia os bytes
    } else {
        _data.clear();
    }
}

// Acesso aos dados
const unsigned char* Message::data() const {
    // Retorna nullptr se não houver dados
    return _data.empty() ? nullptr : _data.data();
}

// Pega o tamanho dos dados
std::size_t Message::size() const {
    return _data.size();
} 

