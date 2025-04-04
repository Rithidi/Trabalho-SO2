#pragma once
#include <semaphore.h>
#include <pthread.h>
#include <list>
#include <queue>
#include <memory>
#include <algorithm>

// Declaração antecipada da classe Observer
template<typename D, typename C>
class Concurrent_Observer;

// Classe que representa um objeto que pode ser observado (Observed)
template<typename D, typename C = void>
class Concurrent_Observed {
public:
    using DataType = D;               // Tipo dos dados transmitidos
    using ConditionType = C;          // Tipo da condição (ex: endereço)
    using ObserverPair = std::pair<Concurrent_Observer<D, C>*, C>;  // Par (observador, condição)

    Concurrent_Observed() {
        pthread_mutex_init(&_mutex, nullptr); // Inicializa o mutex
    }

    ~Concurrent_Observed() {
        pthread_mutex_destroy(&_mutex); // Destroi o mutex
    }

    // Associa um observador a uma condição (ex: endereço de destino)
    bool attach(Concurrent_Observer<D, C>* o, C c) {
        pthread_mutex_lock(&_mutex);
        // Verifica se já está registrado
        auto it = std::find_if(_observers.begin(), _observers.end(),
            [o, c](const auto& pair) { return pair.first == o && pair.second == c; });

        if (it == _observers.end()) {
            // Adiciona se ainda não existir
            _observers.emplace_back(o, c);
            // Ordena com base na condição (pode ser útil para prioridades)
            _observers.sort([](const auto& a, const auto& b) {
                return a.second < b.second;
            });
        }
        pthread_mutex_unlock(&_mutex);
        return it == _observers.end();
    }

    // Remove um observador da lista
    void detach(Concurrent_Observer<D, C>* o, C c) {
        pthread_mutex_lock(&_mutex);
        _observers.remove_if([o, c](const auto& pair) {
            return pair.first == o && pair.second == c;
        });
        pthread_mutex_unlock(&_mutex);
    }

    // Notifica todos os observadores associados à condição 'c'
    bool notify(C c, D* d) {
        pthread_mutex_lock(&_mutex);
        bool notified = false;
        for (auto& pair : _observers) {
            if (pair.second == c) {
                pair.first->update(c, d); // Chama update no observer
                notified = true;
            }
        }
        pthread_mutex_unlock(&_mutex);
        return notified;
    }

private:
    std::list<ObserverPair> _observers;  // Lista de observadores com suas condições
    pthread_mutex_t _mutex;              // Mutex para garantir acesso seguro
};

// Classe que representa um observador concorrente (Observer)
template<typename D, typename C>
class Concurrent_Observer {
public:
    using DataType = D;
    using ConditionType = C;

    Concurrent_Observer() {
        sem_init(&_semaphore, 0, 0);              // Inicializa o semáforo
        pthread_mutex_init(&_mutex, nullptr);     // Inicializa o mutex
    }

    ~Concurrent_Observer() {
        pthread_mutex_lock(&_mutex);
        while (!_data.empty()) _data.pop();       // Limpa a fila de dados
        pthread_mutex_unlock(&_mutex);
        pthread_mutex_destroy(&_mutex);           // Destroi o mutex
        sem_destroy(&_semaphore);                 // Destroi o semáforo
    }

    // Função chamada quando o observado envia uma notificação
    void update(C c, D* d) {
        pthread_mutex_lock(&_mutex);
        _data.push(std::unique_ptr<D>(d));        // Armazena os dados recebidos
        pthread_mutex_unlock(&_mutex);
        sem_post(&_semaphore);                    // Sinaliza que há novos dados
    }

    // Bloqueia até que dados estejam disponíveis e os retorna
    std::unique_ptr<D> updated() {
        sem_wait(&_semaphore);                    // Aguarda sinal do semáforo
        pthread_mutex_lock(&_mutex);
        auto item = std::move(_data.front());     // Obtém o item da fila
        _data.pop();
        pthread_mutex_unlock(&_mutex);
        return item;
    }

    // Verifica se a fila de dados está vazia
    bool empty() const {
        pthread_mutex_lock(&_mutex);
        bool is_empty = _data.empty();
        pthread_mutex_unlock(&_mutex);
        return is_empty;
    }

private:
    sem_t _semaphore;                             // Semáforo para sincronização
    mutable pthread_mutex_t _mutex;               // Mutex para acesso à fila
    std::queue<std::unique_ptr<D>> _data;         // Fila com os dados recebidos
};
