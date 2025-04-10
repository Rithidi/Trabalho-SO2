#pragma once

#include "observer.hpp"
#include "nic.hpp"
#include "message.hpp"


class Protocol : public Concurrent_Observed {
   public:
      int protocol_number;


      Protocol(NIC* nic, int protocol_number) {
         this->_nic = nic;
         this->protocol_number = protocol_number;
         this->_data_observed = Conditional_Data_Observer(this, protocol_number);
         _nic->attach(_data_observed, protocol_number);
      };


      ~Protocol() {
         _nic->detach(_observed, this->protocol_number);
      };

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
      
      void update(typename NIC::Observed * obs, NIC::Protocol_Number prot, Buffer * buf) {
         
         // Obtem a mensagem do buffer

         // notifica observador para repassar mensagem para os comunicadores.

         //_observed.notify(message)

         // libera buffer da NIC

         // _nic->free(buf);
         

         /*
         if(!_observed.notify(buf)) // to call receive(...);
         _nic->free(buf);
         */
      }

   private:
      NIC* _nic;
      Conditional_Data_Observer _data_observed;
};