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


class Conditional_Data_Observer;


class Conditional_Data_Observed;