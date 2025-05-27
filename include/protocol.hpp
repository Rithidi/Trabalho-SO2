#pragma once

#include <cstring> // memcpy

#include "observer.hpp"
#include "nic.hpp"
#include "ethernet.hpp"
#include "message.hpp"
#include "data_publisher.hpp"
#include "time_sync_manager.hpp"

using Protocol_Number = Ethernet::Protocol_Number;
using Address = Ethernet::Address;
using Type = Ethernet::Type;
using Period = Ethernet::Period;

// Forward declaration para evitar inclusão circular
class DataPublisher; 

class Protocol {
public:
    typedef NIC<Engine>::Buffer Buffer;

    Protocol_Number protocol_number;

    Protocol(NIC<Engine>* nic, DataPublisher* data_publisher, Protocol_Number protocol_number);
    ~Protocol();

    int send(Address from, Address to, Type type, Period period, const void* data, unsigned int size);
    void receive(void* buf);
    void attach(Concurrent_Observer* obs);
    void detach(Concurrent_Observer* obs);

private:
    NIC<Engine>* _nic;
    DataPublisher* _data_publisher;
    TimeSyncManager _time_sync_manager;
    Conditional_Data_Observer _data_observer;
    Concurrent_Observed _observed;
};