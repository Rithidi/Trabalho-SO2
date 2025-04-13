#include "../include/observer.hpp"
#include "../include/protocol.hpp"


#include <iostream>


Concurrent_Observer::Concurrent_Observer() {
    sem_init(&semaphore, 0, 0);  // Inicializa o semáforo com 0
}

void Concurrent_Observer::update(Message message) {
    mutex.lock();
    _message_buffer.push(message);
    mutex.unlock();

    // Notifica que novos dados estão disponíveis
    sem_post(&semaphore);
}

Message Concurrent_Observer::updated() {

    //std::cout << "Communicador esperando por mensagem zzz" << std::endl;

    // Espera até que novos dados estejam disponíveis
    sem_wait(&semaphore);

    //std::cout << "Communicador recebeu mensagem !!!" << std::endl;

    mutex.lock();
    Message message = _message_buffer.front();
    _message_buffer.pop();  // Remove o elemento da fila
    mutex.unlock();

    return message;
}

void Concurrent_Observed::attach(Concurrent_Observer* observer) {
    mutex.lock();
    observers.emplace_back(observer);
    mutex.unlock();
}

void Concurrent_Observed::detach(Concurrent_Observer* observer) {
    mutex.lock();
    observers.remove(observer);  // Remove o observador da lista
    mutex.unlock();
}

void Concurrent_Observed::notify(Address adr, Message message) {
    mutex.lock();
    for (Concurrent_Observer* obs : observers) {
        if (obs->communicator_address.mac_address == adr.mac_address && obs->communicator_address.port == adr.port) {
            obs->update(message);  // Atualiza o observador com os dados
        }
    }
    mutex.unlock();
}

Conditional_Data_Observer::Conditional_Data_Observer(Protocol* protocol, Protocol_Number protocol_number)
    : _protocol(protocol), protocol_number(protocol_number) {}

void Conditional_Data_Observer::update(void* buffer) {
    _protocol->receive(buffer);  // Chama update da classe Protocol para processar pacote recebido.
}

void Conditional_Data_Observed::attach(Conditional_Data_Observer* obs) {
    data_observers.emplace_back(obs);  // Adiciona o observador na lista
}

void Conditional_Data_Observed::detach(Conditional_Data_Observer* obs) {
    data_observers.remove(obs);  // Remove o observador da lista
}

void Conditional_Data_Observed::notify(Protocol_Number protocol_number, void* buffer) {
    for (Conditional_Data_Observer* data_obs : data_observers) {
        if (data_obs->protocol_number == protocol_number) {
            data_obs->update(buffer);  // Atualiza o observador com o endereço do buffer
        }
    }
}
