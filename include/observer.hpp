#pragma once

#include <list>
#include <mutex>
#include <queue>
#include <semaphore.h>
#include <array>

#include "message.hpp"
#include "ethernet.hpp"

// Forward declarations
template<typename> class NIC;
class Protocol;

using Protocol_Number = Ethernet::Protocol_Number;
using Address = Ethernet::Address;

class Concurrent_Observer {
public:
    Address communicator_address;

    // Construtor
    Concurrent_Observer();

    void update(Message message);
    Message updated();

    // Retorna se a mensagens na fila.
    bool hasMessage();

private:
    sem_t semaphore;
    std::queue<Message> _message_buffer;
    std::mutex mutex;
};

class Concurrent_Observed {
public:
    void attach(Concurrent_Observer* observer);
    void detach(Concurrent_Observer* observer);
    void notify(Message message);

private:
    std::list<Concurrent_Observer*> observers;
    std::mutex mutex;
};

class Conditional_Data_Observer {
public:
    Protocol_Number protocol_number;
    Conditional_Data_Observer(Protocol* protocol, Protocol_Number protocol_number);
    void update(void* buffer, bool is_internal);

private:
    Protocol* _protocol;
};

class Conditional_Data_Observed {
public:
    void attach(Conditional_Data_Observer* obs);
    void detach(Conditional_Data_Observer* obs);
    void notify(Protocol_Number protocol_number, void* buffer, bool is_internal);

private:
    std::list<Conditional_Data_Observer*> data_observers;
};
