#include "../include/communicator.hpp"
#include "../include/message.hpp"
#include "../include/protocol.hpp"

Communicator::Communicator(Protocol* protocol, Mac_Address vehicle_id, Thread_ID component_id)
    : _protocol(protocol) 
{
    // Inicializa endereço do comunicador: identificador do veiculo (MAC da nic) e identificador do componente.
    _address.vehicle_id = vehicle_id;
    _address.component_id = component_id;
    
    // Inicializa o observador com o endereço do comunicador
    observer.communicator_address.vehicle_id = vehicle_id;
    observer.communicator_address.component_id = component_id;

    // Adiciona o observador à lista de observadores do protocolo
    _protocol->attach(&observer);
}

Communicator::~Communicator() {
    // Desanexa o observador do protocolo quando o comunicador for destruído
    _protocol->detach(&observer);
}

bool Communicator::send(const Message* message) {
    return (_protocol->send(_address, message->getDstAddress(), message->getType(),
            message->getPeriod(), message->data(), message->size()) > 0);
}

bool Communicator::receive(Message* message) {
    // Aguarda até que uma mensagem seja recebida
    // Chama o método updated() do observador para bloquear até receber uma mensagem
    Message received_message = observer.updated();

    // Extrai o cabeçalho da mensagem recebida
    message->setSrcAddress(received_message.getSrcAddress());
    message->setDstAddress(received_message.getDstAddress());
    message->setType(received_message.getType());
    message->setPeriod(received_message.getPeriod());
    message->setTimestamp(received_message.getTimestamp());
    
    // Copia o conteúdo da mensagem recebida para a mensagem do comunicador.
    message->setData(received_message.data(), received_message.size());

    return true;
}

bool Communicator::hasMessage() {
    // Verifica se há mensagens disponíveis no observador
    return observer.hasMessage();
}

Concurrent_Observer* Communicator::getObserver() {
    return &observer;
}
