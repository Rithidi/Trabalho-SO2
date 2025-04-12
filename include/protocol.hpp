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

      Protocol(NIC<Engine>* nic, Protocol_Number protocol_number) 
         : _nic(nic), protocol_number(protocol_number), _data_observer(this, protocol_number) // Inicializa o observador com o número do protocolo
      {
         _nic->attach(&_data_observer);
      };

      ~Protocol() {
         _nic->detach(&_data_observer);
      };

   public:
      int send(Address from, Address to, const void* data, unsigned int size) {
         // Aloca buffer para o cabeçalho + dados
         Buffer* buf = _nic->alloc(to, protocol_number, size + sizeof(Ethernet::Header) + 14); // tamanho: mensagem + cabecalho_aplicacao + cabecalho_ethernet

         // Verifica se o buffer foi alocado corretamente
         if (buf == nullptr) return -1;

         // Preenche cabeçalho da aplicacao no frame Ethernet
         buf->frame.header.src_address = from;
         buf->frame.header.dst_address = to;

         // Copia os dados da mensagem para o payload do frame Ethernet
         std::memcpy(buf->frame.payload, data, size);

         // Envia o frame pela NIC
         return _nic->send(buf);
      }

      void receive(Buffer* buf) {
         // Endereco de destino do frame recebido
         Address dst = buf->frame.header.dst_address;
         // Endereco de origem do frame recebido
         Address src = buf->frame.header.src_address;

         // Tamanho do payload recebido
         unsigned int payload_size = buf->size - sizeof(Ethernet::Header) - 14; // tamanho: mensagem + cabecalho_aplicacao + cabecalho_ethernet
         
         Message message; // Mensagem a ser enviada para o observador
         message.setData(buf->frame.payload, payload_size); // Copia os dados do payload para a mensagem

         _observed.notify(dst, message); // Notifica os observadores com o endereço de destino e a mensagem recebida

         // Libera o buffer alocado para o frame
         _nic->free(buf);
      }

      void attach(Concurrent_Observer* obs) {
         _observed.attach(obs);
      }

      void detach(Concurrent_Observer* obs) {
         _observed.detach(obs);
      }
  
   private:
      NIC<Engine>* _nic;
      Conditional_Data_Observer _data_observer;
      Concurrent_Observed _observed;
};