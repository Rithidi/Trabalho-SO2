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

    /*
    // Verifica se encaminha por BROADCAST INTERNO: (todos componentes recebem, menos o que enviou)
    // Utilizado quando: Endereco de destino nao foi preenchido. ((thread_id) == (pthread_t)0)
    if (pthread_equal(dst_address.component_id, (pthread_t)0)) {
        //std::cout << "Observer detectou: Broadcast interno" << std::endl;
        // Notifica o observador de todos os componentes do veiculo menos o que enviou a mensagem.
        for (Concurrent_Observer* obs : observers) {    
            // Verifica se o observador eh diferente do componente que enviou a mensagem.
            // Obs: Pode existir threads com o mesmo ID! por isso verificar o ID do veiculo tambem.
            if (!pthread_equal(obs->communicator_address.component_id, src_address.component_id) ||
                                obs->communicator_address.vehicle_id != src_address.vehicle_id) {
                obs->update(message);
            }
        }
    }
    // Verifica se encaminha por DESTINO ESPECIFICO: (um unico componente recebe)
    // Utilizado quando: Endereco de destino foi preenchido. ((thread_id) != (pthread_t)0)
    else {
        //std::cout << "Observer detectou: Destino especifico (component_id = " << dst_address.component_id << ")" << std::endl;
        for (Concurrent_Observer* obs : observers) {
            // Notifica o observador do componente de id especifico.
            if (obs->communicator_address.component_id == dst_address.component_id) {
                obs->update(message);
                break;
            }
        }
    }
    */
    
    // Verifica se encaminha por DESTINO ESPECIFICO: (um unico componente recebe)
    // Utilizado quando: Endereco de destino foi preenchido. ((thread_id) != (pthread_t)0)
    if (!pthread_equal(dst_address.component_id, (pthread_t)0)) {
        //std::cout << "Observer detectou: Destino especifico (component_id = " << dst_address.component_id << ")" << std::endl;
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
