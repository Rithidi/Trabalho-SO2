#include "nic.hpp"


class Protocol: private NIC::Observer {
public:
 static int send(Address from, Address to, const void * data, unsigned int size);
    // Buffer * buf = NIC::alloc(to.paddr, PROTO, sizeof(Header) + size)
    // NIC::send(buf)

 static int receive(Buffer * buf, Address from, void * data, unsigned int size);
    // unsigned int s = NIC::receive(buf, &from.paddr, &to.paddr, data, size)
    // NIC::free(buf)
    // return s;

 static void attach(Observer * obs, Address address);
 
 static void detach(Observer * obs, Address address);

private:
 void update(typename NIC::Observed * obs, NIC::Protocol_Number prot, Buffer * buf) {
    if(!_observed.notify(buf)) // to call receive(...);
    _nic->free(buf);
 }
 

};