#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <vector>

class Message {
public:
    // Construtor principal (recebe bytes + tamanho)
    Message(const unsigned char* data = nullptr, unsigned int size = 0);

    // Define os dados da mensagem
    void set_data(const unsigned char* data, unsigned int size);

    // Retorna ponteiro para os dados (nullptr se vazio)
    const unsigned char* data() const;

    // Retorna tamanho em bytes
    std::size_t size() const;

private:
    std::vector<unsigned char> _data;
};

#endif // MESSAGE_HPP