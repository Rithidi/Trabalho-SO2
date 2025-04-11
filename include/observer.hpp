#pragma once

#include <list>
#include <mutex>
#include <queue>
#include <semaphore.h>
#include <array>

#include "message.hpp"
#include "protocol.hpp"
#include "ethernet.hpp"


using Protocol_Number = Ethernet::Protocol_Number;
using Address = Ethernet::Address;


class Concurrent_Observer {
public:
    // Endereço do comunicador que está observando.
    Address communicator_address;
public:
    // Construtor: inicializa o endereco e o semáforo
    Concurrent_Observer() {sem_init(&semaphore, 0, 0); } // semáforo inicializado com 0

    void update(Message message) {
        mutex.lock();
        // Adiciona os dados recebidos ao buffer
        _message_buffer.push(message);
        mutex.unlock();
        
        // Incrementa semáforo para notificar que novos dados estão disponíveis
        sem_post(&semaphore);
    }

    Message updated() {
        // Espera até que novos dados estejam disponíveis
        sem_wait(&semaphore);

        mutex.lock();
        // Obtém o último conjunto de dados recebidos
        Message message = _message_buffer.front();
        _message_buffer.pop(); // Remove o elemento da fila
        mutex.unlock();

        return message;
    }

private:
    sem_t semaphore;
    std::queue<Message> _message_buffer; // Fila de mensagens recebidas.
    std::mutex mutex; // Mutex para proteger o acesso à fila de mensagens
};


// Uma instancia para cada 
class Concurrent_Observed {
    public:
        // Adiciona um observador à lista de observadores
        void attach(Concurrent_Observer* observer) {
            mutex.lock();
            observers.emplace_back(observer);
            mutex.unlock();
        }

        // Remove um observador da lista de observadores
        void detach(Concurrent_Observer* observer) {
            mutex.lock();
            observers.remove(observer); // Remove o observador da lista
            mutex.unlock();
        }

        // Notifica todos os observadores com o mesmo numero de porta
        void notify(Address adr, Message message) {
            mutex.lock();
            for (Concurrent_Observer* obs : observers) {
                if (obs->communicator_address.mac_address == adr.mac_address && obs->communicator_address.port == adr.port) {
                    obs->update(message); // Atualiza o observador com os dados
                }
            }
            mutex.unlock();
        }
    
    private:
        std::list<Concurrent_Observer*> observers;
        std::mutex mutex;
};


// Instanciado para cada numero de protocolo diferente que deseja-se observar.
class Conditional_Data_Observer {
    public:
        // Número do protocolo que está observando
        Protocol_Number protocol_number;

        Conditional_Data_Observer(Protocol* protocol, Protocol_Number protocol_number): _protocol(protocol), protocol_number(protocol_number) {}

        void update(void* buffer) {
            // Chama update da classe Protocol para repassar para seu observador.
            _protocol->receive(buffer);
        }

    private:
        Protocol* _protocol;
};


// Uma instancia para comunicar Conditional_Data_Observers quando um frame de um respectivo  num de protocolo é recebido.
class Conditional_Data_Observed {
    public:
        // Adiciona um observador à lista de observadores
        void attach(Conditional_Data_Observer* obs) {
            data_observers.emplace_back(obs); // Adiciona o observador na lista
        }

        // Remove um observador da lista de observadores
        void detach(Conditional_Data_Observer* obs) {
            data_observers.remove(obs); // Remove o observador da lista
        }

        // Notifica todos os observadores com o mesmo numero de protocolo
        void notify(Protocol_Number protocol_number, void* buffer) {
            // Percorre a lista de observadores e notifica o que possue o mesmo número de protocolo
            for (Conditional_Data_Observer* data_obs : data_observers) {
                if (data_obs->protocol_number == protocol_number) {
                    data_obs->update(buffer); // Atualiza o observador com o endereço do buffer
                }
            }
        }
    private:
        std::list<Conditional_Data_Observer*> data_observers;
};