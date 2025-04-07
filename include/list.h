#ifndef LIST_H
#define LIST_H

template<typename T>
class List {
private:
    struct Node {
        T value;
        Node* next;
        Node(const T& v) : value(v), next(nullptr) {}
    };

    Node* _head = nullptr;
    Node* _tail = nullptr;

public:
    ~List() {
        while (_head) {
            Node* tmp = _head;
            _head = _head->next;
            delete tmp;
        }
    }

    void insert(const T& value) {
        Node* node = new Node(value);
        if (_tail) {
            _tail->next = node;
            _tail = node;
        } else {
            _head = _tail = node;
        }
    }

    T remove() {
        if (!_head) return T(); // ou lança exceção
        Node* node = _head;
        T value = node->value;
        _head = node->next;
        if (!_head) _tail = nullptr;
        delete node;
        return value;
    }

    bool empty() const {
        return !_head;
    }
};

#endif
