#pragma once

#include <tuple>
#include <list>
#include <mutex>
#include <vector>
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

    void update(const std::vector<uint8_t>& data) {
        // Adiciona os dados recebidos ao buffer
        data_buffer.push_back(data);
        // Incrementa semáforo para notificar que novos dados estão disponíveis
        sem_post(&semaphore);
    }

    std::vector<uint8_t>* updated() {
        // Espera até que novos dados estejam disponíveis
        sem_wait(&semaphore);
        // Retorna o último conjunto de dados recebidos
        return &data_buffer.back();
    }

private:
    sem_t semaphore;
    std::vector<std::vector<uint8_t>> data_buffer; // Buffer de dados do observador
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
        void notify(int port, const std::vector<uint8_t>& data) {
            mutex.lock();
            for (Concurrent_Observer* observer : observers) {
                if (observer->port == port) {
                    observer->update(data); // Atualiza o observador com os dados
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
        // Construtor: inicializa a porta
        Conditional_Data_Observer(int protocol_number) : protocol_number(protocol_number) {};

        void update(const std::vector<uint8_t>& buffer) {
            // Processa os dados recebidos
            process(buffer);
        }
};

// Uma instancia para comunicar Conditional_Data_Observers quando um frame de um respectivo  num de protocolo é recebido.
class Conditional_Data_Observed {
    public:
        // Adiciona um observador à lista de observadores
        void attach(Conditional_Data_Observer* data_obs) {
            data_observers.emplace_back(data_obs); // Adiciona o observador na lista
        }

        // Remove um observador da lista de observadores
        void detach(Conditional_Data_Observer* data_obs) {
            data_observers.remove(data_obs); // Remove o observador da lista
        }

        // Notifica todos os observadores com o mesmo numero de protocolo
        void notify(int protocol_number, const std::vector<uint8_t>& buffer) {
            for (Conditional_Data_Observer* data_obs : data_observers) {
                if (data_obs->protocol_number == protocol_number) {
                    data_obs->update(buffer); // Atualiza o observador com o endereço do buffer
                }
            }
        }
    private:
        std::list<Conditional_Data_Observer*> data_observers;
};