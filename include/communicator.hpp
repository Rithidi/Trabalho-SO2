#pragma once

#include "message.hpp"
#include "observer.hpp"
#include "protocol.hpp"
#include <array>

using Address = Ethernet::Address;
using Port = Ethernet::Port;

class Communicator {
public:
    // Construtor: inicializa o comunicador com o protocolo, endereço MAC e porta
    Communicator(Protocol* protocol, std::array<uint8_t, 6> mac_address, Port port);

    // Destrutor: desanexa o observador do protocolo
    ~Communicator();

    // Envia uma mensagem para o destino especificado
    bool send(const Message* message, Address destination);

    // Recebe uma mensagem e coloca no parâmetro message
    bool receive(Message* message);

private:
    Protocol* _protocol;  // Ponteiro para o protocolo utilizado
    Address _address;     // Endereço (MAC e Porta) do comunicador
    Concurrent_Observer observer;  // Observador para receber as mensagens
};
