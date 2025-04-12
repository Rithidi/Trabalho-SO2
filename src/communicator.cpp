#include "../include/communicator.hpp"

Communicator::Communicator(Protocol* protocol, std::array<uint8_t, 6> mac_address, Port port)
    : _protocol(protocol) 
{
    // Inicializa o endereço MAC e a porta do comunicador
    _address.mac_address = mac_address;
    _address.port = port;
    
    // Inicializa o observador com o endereço do comunicador
    observer.communicator_address.mac_address = mac_address;
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
