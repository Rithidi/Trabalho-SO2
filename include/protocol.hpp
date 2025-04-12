#pragma once

#include <cstring> // memcpy

#include "observer.hpp"
#include "nic.hpp"
#include "ethernet.hpp"
#include "message.hpp"

using Protocol_Number = Ethernet::Protocol_Number;
using Address = Ethernet::Address;

class Protocol {
public:
    typedef NIC<Engine>::Buffer Buffer;

    Protocol_Number protocol_number;

    Protocol(NIC<Engine>* nic, Protocol_Number protocol_number);
    ~Protocol();

    int send(Address from, Address to, const void* data, unsigned int size);
    void receive(void* buf);
    void attach(Concurrent_Observer* obs);
    void detach(Concurrent_Observer* obs);

private:
    NIC<Engine>* _nic;
    Conditional_Data_Observer _data_observer;
    Concurrent_Observed _observed;
};