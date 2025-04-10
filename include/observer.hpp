#pragma once

#include <list>
#include <mutex>
#include <queue>
#include <semaphore.h>


class Concurrent_Observer {
public:
    // Porta do observador
    int port;
public:
    // Construtor: inicializa a porta e o semáforo
    Concurrent_Observer(int port) {
        this->port = port; // Inicializa a porta do observador
        sem_init(&semaphore, 0, 0); // Inicializa o semáforo com valor 0
    }

    void update(Message message) {
        // Adiciona os dados recebidos ao buffer
        _message_buffer.push(message);
        // Incrementa semáforo para notificar que novos dados estão disponíveis
        sem_post(&semaphore);
    }

    Message updated() {
        // Espera até que novos dados estejam disponíveis
        sem_wait(&semaphore);
        // Obtém o último conjunto de dados recebidos
        Message message = _message_buffer.front();
        _message_buffer.pop(); // Remove o elemento da fila
        return message;
    }

private:
    sem_t semaphore;
    std::queue<Message> _message_buffer; // Fila de mensagens recebidas.
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
        void notify(int port, Message message) {
            mutex.lock();
            for (Concurrent_Observer* observer : observers) {
                if (observer->port == port) {
                    observer->update(message); // Atualiza o observador com os dados
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
        // Número de protocolo usado para identificar o tipo de dados que o observador deve processar
        int protocol_number;
        Conditional_Data_Observer(Protocol* protocol, int protocol_number): _protocol(protocol), protocol_number(protocol_number) {}


        void update(int protocol_number, void* buffer) {
            // Chama update da classe Protocol para repassar para seu observador.
            _protocol->update(this, protocol_number, buffer);
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
        void notify(int protocol_number, void* buffer) {
            // Percorre a lista de observadores e notifica o que possue o mesmo número de protocolo
            for (Conditional_Data_Observer* data_obs : data_observers) {
                if (data_obs->protocol_number == protocol_number) {
                    data_obs->update(protocol_number, buffer); // Atualiza o observador com o endereço do buffer
                }
            }
        }
    private:
        std::list<Conditional_Data_Observer*> data_observers;
};