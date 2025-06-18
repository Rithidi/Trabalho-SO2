#pragma once

#include <cstring> // memcpy

#include "observer.hpp"
#include "nic.hpp"
#include "ethernet.hpp"
#include "message.hpp"
#include "data_publisher.hpp"
#include "time_sync_manager.hpp"
#include "rsu_handler.hpp"

using Protocol_Number = Ethernet::Protocol_Number;
using Address = Ethernet::Address;
using Type = Ethernet::Type;
using Period = Ethernet::Period;
using MAC_key = Ethernet::MAC_key;
using Quadrant_ID = Ethernet::Quadrant_ID;

// Forward declaration para evitar inclusão circular
class DataPublisher; 

class Protocol {
public:
    typedef NIC<Engine>::Buffer Buffer;

    Protocol_Number protocol_number;

    Protocol(NIC<Engine>* nic, DataPublisher* data_publisher, Protocol_Number protocol_number,
            RSUHandler* rsu_handler = nullptr, TimeSyncManager* tsm = nullptr);
    ~Protocol();

    int send(Address from, Address to, Type type, Period period, Quadrant_ID group_id, MAC_key mac, const void* data, unsigned int size);
    void receive(void* buf, bool is_internal);

    void attach(Concurrent_Observer* obs);
    void detach(Concurrent_Observer* obs);

private: 
    void processInternalSend(Ethernet::InternalPayload* payload, Ethernet::Thread_ID src_component, Ethernet::Thread_ID dst_component, Type type, Period period);
    void processExternalSend(Ethernet::ExternalPayload* payload, Address from, Address to, Type type, Period period, Quadrant_ID group_id, MAC_key mac);

    void processInternalReceive(Ethernet::InternalPayload payload);
    void processExternalReceive(Ethernet::ExternalPayload payload);

private:
    NIC<Engine>* _nic;

    DataPublisher* _data_publisher;
    TimeSyncManager* _time_sync_manager;
    RSUHandler* _rsu_handler;

    Conditional_Data_Observer _data_observer;
    Concurrent_Observed _observed;
};