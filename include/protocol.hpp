#pragma once
#include "observer.hpp"
#include "message.hpp"

template <typename NIC>
class Protocol : private typename NIC::Observer
{
public:
    static const typename NIC::Protocol_Number PROTO = Traits<Protocol>::ETHERNET_PROTOCOL_NUMBER;
    
    typedef typename NIC::Buffer Buffer;
    typedef typename NIC::Address Physical_Address;
    typedef uint16_t Port;
    
    typedef Conditional_Data_Observer<Buffer, Port> Observer;
    typedef Conditionally_Data_Observed<Buffer, Port> Observed;

    class Address {
    public:
        enum Null { BROADCAST };
        
        Address() : _paddr(), _port(0) {}
        Address(const Null& null) : _paddr(null), _port(0) {}
        Address(Physical_Address paddr, Port port) : _paddr(paddr), _port(port) {}
        
        operator bool() const { return (_paddr || _port); }
        bool operator==(const Address& a) const { return (_paddr == a._paddr) && (_port == a._port); }
        
        Physical_Address physical_address() const { return _paddr; }
        Port port() const { return _port; }
        
    private:
        Physical_Address _paddr;
        Port _port;
    };

    class Header {
    public:
        Physical_Address src_paddr;
        Physical_Address dst_paddr;
        Port src_port;
        Port dst_port;
        uint16_t length;
    } __attribute__((packed));

    static const unsigned int MTU = NIC::MTU - sizeof(Header);

    class Packet : public Header {
    public:
        Header* header() { return this; }
        template<typename T> T* data() { return reinterpret_cast<T*>(&_data); }
    private:
        uint8_t _data[MTU];
    } __attribute__((packed));

    Protocol(NIC* nic) : _nic(nic), _observed(new Observed()) {
        _nic->attach(this, PROTO);
    }

    ~Protocol() {
        _nic->detach(this, PROTO);
        delete _observed;
    }

    int send(Address from, Address to, const void* data, unsigned int size) {
        if(size > MTU) return -1;
        
        Buffer* buf = _nic->alloc(to.physical_address(), PROTO, sizeof(Header) + size);
        if(!buf) return -2;
        
        Packet* packet = reinterpret_cast<Packet*>(buf->data());
        packet->src_paddr = from.physical_address();
        packet->dst_paddr = to.physical_address();
        packet->src_port = from.port();
        packet->dst_port = to.port();
        packet->length = htons(size);
        
        memcpy(packet->data(), data, size);
        return _nic->send(buf);
    }

    int receive(Buffer* buf, Address* from, void* data, unsigned int size) {
        const Packet* packet = reinterpret_cast<const Packet*>(buf->data());
        unsigned int packet_size = ntohs(packet->length);
        
        if(packet_size > size) {
            _nic->free(buf);
            return -1;
        }
        
        if(from) {
            *from = Address(packet->src_paddr, packet->src_port);
        }
        
        memcpy(data, packet->data(), packet_size);
        _nic->free(buf);
        return packet_size;
    }

    void attach(Observer* obs, Address address) {
        _observed->attach(obs, address.port());
    }

    void detach(Observer* obs, Address address) {
        _observed->detach(obs, address.port());
    }

private:
    void update(typename NIC::Observed* obs, NIC::Protocol_Number prot, Buffer* buf) {
        const Packet* packet = reinterpret_cast<const Packet*>(buf->data());
        if(!_observed->notify(packet->dst_port, buf)) {
            _nic->free(buf);
        }
    }

    NIC* _nic;
    Observed* _observed; // Agora por instância
};