#include "../include/communicator.hpp"

Communicator::Communicator(Protocol* protocol, std::array<uint8_t, 6> vehicle_id, Thread_ID component_id, Port port)
    : _protocol(protocol) 
{
    // Inicializa o endereço MAC, identificador do componente e porta do comunicador.
    _address.vehicle_id = vehicle_id;
    _address.component_id = component_id;
    _address.port = port;
    
    // Inicializa o observador com o endereço do comunicador
    observer.communicator_address.vehicle_id = vehicle_id;
    observer.communicator_address.component_id = component_id;
    observer.communicator_address.port = port;

    // Adiciona o observador à lista de observadores do protocolo
    _protocol->attach(&observer);
}

Communicator::~Communicator() {
    // Desanexa o observador do protocolo quando o comunicador for destruído
    _protocol->detach(&observer);
}

bool Communicator::send(const Message* message, Address destination) {
    // Envia a mensagem usando o protocolo
    return (_protocol->send(_address, destination, message->data(), message->size()) > 0);
}

bool Communicator::receive(Message* message) {
    // Aguarda até que uma mensagem seja recebida
    // Chama o método updated() do observador para bloquear até receber uma mensagem
    Message received_message = observer.updated();
    
    // Verifica se a mensagem recebida tem tamanho maior que zero.
    if (received_message.size() == 0) {
        return false;
    }

    // Copia o conteúdo da mensagem recebida para a mensagem do comunicador.
    message->setData(received_message.data(), received_message.size());

    return true;
}
