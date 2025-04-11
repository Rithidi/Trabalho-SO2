#pragma once

#include <cstring> // memcpy

#include "observer.hpp"
#include "nic.hpp"
#include "ethernet.hpp"
#include "message.hpp"


using Protocol_Number = Ethernet::Protocol_Number;

class Protocol : public Concurrent_Observed {
   public:

   typedef NIC::Buffer Buffer;

   Protocol_Number protocol_number;

      Protocol(NIC* nic, Protocol_Number protocol_number) {
         this->_nic = nic;
         this->protocol_number = protocol_number;
         this->_data_observed = Conditional_Data_Observer(this, protocol_number);
         _nic->attach(_data_observed, protocol_number);
      };

      ~Protocol() {
         _nic->detach(_data_observed, this->protocol_number);
      };

   public:
      int send(Address from, Address to, const void * data, unsigned int size) {
         // Aloca buffer para o cabeçalho + dados
         Buffer* buf = nic->alloc(to, protocol_number, size + sizeof(Ethernet::Header) + 14); // tamanho: mensagem + cabecalho_aplicacao + cabecalho_ethernet

         // Verifica se o buffer foi alocado corretamente
         if (buf == nullptr) return -1;

         // Preenche cabeçalho da aplicacao no frame Ethernet
         buf->frame.header.src_address = from;
         buf->frame.header.dst_address = to;

         // Copia os dados da mensagem para o payload do frame Ethernet
         std::memcpy(buf->frame.payload, data, size);

         // Envia o frame pela NIC
         return nic->send(buf);
      }

      static int receive(Buffer * buf, Address from, void * data, unsigned int size) {
         
     }

     void update(typename NIC::Observed * obs, NIC::Protocol_Number prot, Buffer * buf) {

      }
  
   private:
      NIC* _nic;
      Conditional_Data_Observer _data_observed;
};