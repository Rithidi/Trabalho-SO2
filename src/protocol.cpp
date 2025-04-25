#include "../include/protocol.hpp"
#include "../include/ethernet.hpp"

#include <iostream>

Protocol::Protocol(NIC<Engine>* nic, Protocol_Number protocol_number) 
    : _nic(nic), protocol_number(protocol_number), _data_observer(this, protocol_number)
{
    _nic->attach(&_data_observer);
};

Protocol::~Protocol() {
    _nic->detach(&_data_observer);
};

int Protocol::send(Address from, Address to, const void* data, unsigned int size) {
    // Aloca buffer para o cabeçalho + dados (cabeçalho Ethernet + payload)
    Buffer* buf = _nic->alloc(to, protocol_number, sizeof(Ethernet::Frame));

    // Verifica se o buffer foi alocado corretamente
    if (buf == nullptr) return -1;

    // Preenche o cabeçalho do frame Ethernet
    buf->frame.src = from.vehicle_id; // Endereço de origem
    buf->frame.type = htons(0x88B5);   // Tipo do protocolo (customizado)

    // Monta Payload com o cabeçalho e a mensagem
    Ethernet::Payload payload;
    payload.header.src_address = from; // Endereço de origem
    payload.header.dst_address = to;   // Endereço de destino
    std::memcpy(payload.data, data, size); // Dados da mensagem

    // Preenche o payload do frame com os dados.
    if (!_nic->fillPayload(&buf->frame, &payload)) {
        return -1; // Retorna -1 se o preenchimento falhar
    }   

    bool is_internal = false;

    // Verifica se o endereço MAC de origem e destino são iguais
    if (from.vehicle_id == to.vehicle_id) {
        is_internal = true; // Define como interno se os endereços forem iguais
    } 
    
    // Envia o frame Ethernet
    return _nic->send(buf, is_internal);
}

void Protocol::receive(void* buf) {
    // Conversão direta de void* para Buffer*
    Buffer* buffer = static_cast<Buffer*>(buf);

    // Inicializa Payload para receber os dados.
    Ethernet::Payload payload;

    // Chama o método extractPayload para preencher o cabeçalho e a mensagem
    _nic->extractPayload(&buffer->frame, &payload);

    // Libera o buffer após o uso
    delete buffer;

    // Obtém os endereços de origem e destino do cabeçalho
    Address dst = payload.header.dst_address; // Endereço de destino
    Address src = payload.header.src_address; // Endereço de origem

    Message msg;
    msg.setData(payload.data, sizeof(payload.data)); // Copia os dados para a mensagem

    // Notifica os observadores com o endereço de destino e a mensagem
    _observed.notify(dst, msg);
}


void Protocol::attach(Concurrent_Observer* obs) {
    _observed.attach(obs);
}

void Protocol::detach(Concurrent_Observer* obs) {
    _observed.detach(obs);
}
