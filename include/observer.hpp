#pragma once
#include <semaphore.h>  // Para semáforos POSIX
#include <list>
#include <queue>
#include <utility> // para std::pair

// Forward declaration
template<typename D, typename C = void>
class Concurrent_Observer;

//---------------------------------------------------------
// Concurrent_Observed (Subject)
//---------------------------------------------------------
template<typename D, typename C = void>
class Concurrent_Observed {
    friend class Concurrent_Observer<D, C>;
public:
    typedef D Observed_Data;
    typedef C Observing_Condition;
    
    void attach(Concurrent_Observer<D, C>* o, C c) {
        _observers.emplace_back(o, c); // Insere no final
        _observers.sort([](const auto& a, const auto& b) {
            return a.second < b.second; // Ordena por condição
        });
    }
    
    void detach(Concurrent_Observer<D, C>* o, C c) {
        _observers.remove_if([o, c](const auto& pair) {
            return pair.first == o && pair.second == c;
        });
    }
    
    bool notify(C c, D* d) {
        bool notified = false;
        for (auto& [observer, condition] : _observers) {
            if (condition == c) {
                observer->update(c, d);
                notified = true;
            }
        }
        return notified;
    }

private:
    std::list<std::pair<Concurrent_Observer<D, C>*, C>> _observers; // Lista ordenada
};

//---------------------------------------------------------
// Concurrent_Observer (Observer)
//---------------------------------------------------------
template<typename D, typename C = void>
class Concurrent_Observer {
    friend class Concurrent_Observed<D, C>;
public:
    typedef D Observed_Data;
    typedef C Observing_Condition;
    
    Concurrent_Observer() {
        sem_init(&_semaphore, 0, 0); // Inicializado bloqueado
    }
    
    ~Concurrent_Observer() {
        sem_destroy(&_semaphore);
        // Limpa a fila se necessário
        while (!_data.empty()) {
            delete _data.front();
            _data.pop();
        }
    }
    
    void update(C c, D* d) {
        _data.push(d);
        sem_post(&_semaphore); // Libera uma thread
    }
    
    D* updated() {
        sem_wait(&_semaphore); // Bloqueia até receber dados
        D* item = _data.pop();
        return item;
    }

private:
    sem_t _semaphore;
    std::queue<D*> _data; // FIFO
};