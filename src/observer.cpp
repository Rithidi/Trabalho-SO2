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
    // Espera até que novos dados estejam disponíveis
    sem_wait(&semaphore);

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
    // Verifica se encaminha via BROADCAST INTERNO: (todos componentes na porta destino)
    // Nao possui ID do componente de destino ((thread_id) == (pthread_t)0)
    if (pthread_equal(adr.component_id, (pthread_t)0)) {
        for (Concurrent_Observer* obs : observers) {
            // Notifica todos os observadores associados a porta do endereco de destino.
            if (obs->communicator_address.port == adr.port) {
                obs->update(message);  // Atualiza o observador com os dados
            }
        }
    // Verifica se encaminha via DESTINO ESPECIFICO: (unico componente)
    // Possui ID do componente de destino ((thread_id) != (pthread_t)0)
    } else {
        for (Concurrent_Observer* obs : observers) {
            // Notifica o observador associado ao ID e porta do endereco de destino.
            if (obs->communicator_address.component_id == adr.component_id && obs->communicator_address.port == adr.port) {
                obs->update(message);  // Atualiza o observador com os dados
                break;  // Para de notificar após encontrar o observador correspondente
            }
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
