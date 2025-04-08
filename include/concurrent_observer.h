#ifndef CONCURRENT_OBSERVER_H
#define CONCURRENT_OBSERVER_H

#include "semaphore.h"
#include "list.h"

// Forward declaration
template<typename D, typename C = void>
class Concurrent_Observer;

template<typename D, typename C>
class Concurrent_Observed
{
    friend class Concurrent_Observer<D, C>;

public:
    typedef D Observed_Data;
    typedef C Observing_Condition;
    typedef Ordered_List<Concurrent_Observer<D, C>, C> Observers;

    Concurrent_Observed() {}
    ~Concurrent_Observed() {}

    void attach(Concurrent_Observer<D, C>* o, C c) {
        o->set_rank(c);      
        _observers.insert(o);
    }
    

    void detach(Concurrent_Observer<D, C>* o, C c) {
        _observers.remove(o);
    }

    bool notify(C c, D* d) {
        bool notified = false;
        for (auto obs = _observers.begin(); obs != _observers.end(); ++obs) {
            if (obs->rank() == c) {
                obs->update(c, d);
                notified = true;
            }
        }
        return notified;
    }

private:
    Observers _observers;
};

template<typename D, typename C>
class Concurrent_Observer
{
    friend class Concurrent_Observed<D, C>;

public:
    typedef D Observed_Data;
    typedef C Observing_Condition;

    Concurrent_Observer() : _semaphore(0) {}

    ~Concurrent_Observer() {}

    D* updated() {
        _semaphore.p();
        return _data.remove();
    }

    C rank() const { return _rank; }

private:
    void update(C c, D* d) {
        D* copy = new D(*d);
        _data.insert(copy);
        _semaphore.v();
    }

    void set_rank(C c) { _rank = c; }

private:
    C _rank; 
    Semaphore _semaphore;
    List<D> _data;
};


#endif 
