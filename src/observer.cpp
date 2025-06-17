#include "../include/observer.hpp"
#include "../include/protocol.hpp"
#include "../include/ethernet.hpp"


#include <iostream>


Concurrent_Observer::Concurrent_Observer() {
    sem_init(&semaphore, 0, 0);  // Inicializa o semáforo com 0
}

bool Concurrent_Observer::hasMessage() {
    std::lock_guard<std::mutex> lock(mutex); // Protege o acesso à fila de mensagens
    return !_message_buffer.empty();  // Verifica se a fila de mensagens não está vazia
}

void Concurrent_Observer::update(Message message) {
    mutex.lock();
    _message_buffer.push(message); // Adiciona mensagem na fila
    mutex.unlock();

    // Notifica que novos dados estão disponíveis
    sem_post(&semaphore);
}

Message Concurrent_Observer::updated() {
    // Espera até que alguma mensagem esteja disponível
    sem_wait(&semaphore);

    mutex.lock();
    Message message = _message_buffer.front(); // Obtém o primeiro elemento da fila
    _message_buffer.pop();  // Remove o elemento da fila
    mutex.unlock();

    return message; // Retorna a mensagem
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

void Concurrent_Observed::notify(Message message) {
    // Extrai endereco de origem da mensagem.
    Ethernet::Address src_address = message.getSrcAddress();
    // Extrai endereco de destino da mensagem.
    Ethernet::Address dst_address = message.getDstAddress();
    mutex.lock();
    // Utilizado quando: Endereco de destino foi preenchido. ((thread_id) != (pthread_t)0)
    if (!pthread_equal(dst_address.component_id, (pthread_t)0)) {
        for (Concurrent_Observer* obs : observers) {
            // Notifica o observador do componente de id especifico.
            if (obs->communicator_address.component_id == dst_address.component_id) {
                obs->update(message);
                break;
            }
        }
    }
    mutex.unlock();
}

Conditional_Data_Observer::Conditional_Data_Observer(Protocol* protocol, Protocol_Number protocol_number)
    : _protocol(protocol), protocol_number(protocol_number) {}

void Conditional_Data_Observer::update(void* buffer, bool is_internal) {
    _protocol->receive(buffer, is_internal);  // Chama update da classe Protocol para processar pacote recebido.
}

void Conditional_Data_Observed::attach(Conditional_Data_Observer* obs) {
    data_observers.emplace_back(obs);  // Adiciona o observador na lista
}

void Conditional_Data_Observed::detach(Conditional_Data_Observer* obs) {
    data_observers.remove(obs);  // Remove o observador da lista
}

void Conditional_Data_Observed::notify(Protocol_Number protocol_number, void* buffer, bool is_internal) {
    for (Conditional_Data_Observer* data_obs : data_observers) {
        if (data_obs->protocol_number == protocol_number) {
            data_obs->update(buffer, is_internal);  // Atualiza o observador com o endereço do buffer
        }
    }
}
