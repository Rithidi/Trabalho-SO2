#pragma once

#include "message.hpp"
#include "observer.hpp"
#include "ethernet.hpp"

#include <array>

using Mac_Address = Ethernet::Mac_Address;
using Thread_ID = Ethernet::Thread_ID;

class Communicator {
public:
    // Construtor: inicializa o comunicador com o protocolo, endereço MAC e porta
    Communicator(Protocol* protocol, Mac_Address vehicle_id, Thread_ID component_id);

    // Destrutor: desanexa o observador do protocolo
    ~Communicator();

    // Envia uma mensagem
    bool send(Message* message);

    // Recebe uma mensagem
    bool receive(Message* message);

    // Retorna se há mensagens disponíveis
    bool hasMessage();

    Concurrent_Observer* getObserver();
    

private:
    Protocol* _protocol;  // Ponteiro para o protocolo utilizado
    Address _address;     // Endereço (MAC e Porta) do comunicador
    Concurrent_Observer observer;  // Observador para receber as mensagens
};
