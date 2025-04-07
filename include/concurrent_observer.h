#ifndef CONCURRENT_OBSERVER_H
#define CONCURRENT_OBSERVER_H

#include "semaphore.h"
#include "list.h"

template<typename D, typename C = void>
class Concurrent_Observer;

template<typename D, typename C = void>
class Concurrent_Observed {
    friend class Concurrent_Observer<D, C>;

public:
    typedef D Observed_Data;
    typedef C Observing_Condition;

    Concurrent_Observed() {}
    ~Concurrent_Observed() {}

    void attach(Concurrent_Observer<D, C>* o, C c) {
        _observers.insert(o);
    }

    void detach(Concurrent_Observer<D, C>* o, C c) {
        // remove lógica simplificada: remove o primeiro que bater o ponteiro
        List<Concurrent_Observer<D, C>*> new_list;
        while (!_observers.empty()) {
            auto obs = _observers.remove();
            if (obs != o)
                new_list.insert(obs);
        }
        _observers = std::move(new_list);
    }

    bool notify(C c, D* d) {
        bool notified = false;
        // notifica todos (ignora condição, pois é mock inicial)
        List<Concurrent_Observer<D, C>*> copy = _observers;
        while (!copy.empty()) {
            auto obs = copy.remove();
            obs->update(c, d);
            notified = true;
        }
        return notified;
    }

private:
    List<Concurrent_Observer<D, C>*> _observers;
};

template<typename D, typename C>
class Concurrent_Observer {
    friend class Concurrent_Observed<D, C>;

public:
    typedef D Observed_Data;
    typedef C Observing_Condition;

    Concurrent_Observer() : _semaphore(0) {}
    ~Concurrent_Observer() {}

    void update(C c, D* d) {
        _data.insert(d);
        _semaphore.v();
    }

    D* updated() {
        _semaphore.p();
        return _data.remove();
    }

private:
    Semaphore _semaphore;
    List<D*> _data;
};

#endif
