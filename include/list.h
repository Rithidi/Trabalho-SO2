#ifndef LIST_H
#define LIST_H

#include <list>

template <typename T>
class List {
public:
    void insert(T* item) {
        _list.push_back(item);
    }

    T* remove() {
        if (_list.empty()) return nullptr;
        T* item = _list.front();
        _list.pop_front();
        return item;
    }

private:
    std::list<T*> _list;
};

// Ordered_List permite armazenar objetos com "condição"
template <typename T, typename C>
class Ordered_List {
public:
    class Iterator {
    public:
        Iterator(typename std::list<T*>::iterator it) : _it(it) {}
        Iterator& operator++() { ++_it; return *this; }
        bool operator!=(const Iterator& other) const { return _it != other._it; }
        T* operator->() const { return *_it; }

    private:
        typename std::list<T*>::iterator _it;
    };

    void insert(T* t) {
        _list.push_back(t);
    }

    void remove(T* t) {
        _list.remove(t);
    }

    Iterator begin() { return Iterator(_list.begin()); }
    Iterator end() { return Iterator(_list.end()); }

private:
    std::list<T*> _list;
};

#endif // LIST_H
