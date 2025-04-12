#pragma once

#include <list>
#include <mutex>
#include <queue>
#include <semaphore.h>
#include <array>

#include "message.hpp"
#include "protocol.hpp"
#include "ethernet.hpp"

using Protocol_Number = Ethernet::Protocol_Number;
using Address = Ethernet::Address;

class Concurrent_Observer {
public:
    Address communicator_address;

    // Construtor
    Concurrent_Observer();

    void update(Message message);
    Message updated();

private:
    sem_t semaphore;
    std::queue<Message> _message_buffer;
    std::mutex mutex;
};

class Concurrent_Observed {
public:
    void attach(Concurrent_Observer* observer);
    void detach(Concurrent_Observer* observer);
    void notify(Address adr, Message message);

private:
    std::list<Concurrent_Observer*> observers;
    std::mutex mutex;
};

class Conditional_Data_Observer {
public:
    typedef NIC<Engine>::Buffer Buffer;

    Protocol_Number protocol_number;

    Conditional_Data_Observer(Protocol* protocol, Protocol_Number protocol_number);

    void update(Buffer* buffer);

private:
    Protocol* _protocol;
};

class Conditional_Data_Observed {
public:
    typedef NIC<Engine>::Buffer Buffer;

    void attach(Conditional_Data_Observer* obs);
    void detach(Conditional_Data_Observer* obs);
    void notify(Protocol_Number protocol_number, Buffer* buffer);

private:
    std::list<Conditional_Data_Observer*> data_observers;
};
