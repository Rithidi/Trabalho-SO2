#include "../include/protocol.hpp"

Protocol::Protocol(NIC<Engine>* nic, Protocol_Number protocol_number) 
    : _nic(nic), protocol_number(protocol_number), _data_observer(this, protocol_number)
{
    _nic->attach(&_data_observer);
};

Protocol::~Protocol() {
    _nic->detach(&_data_observer);
};

int Protocol::send(Address from, Address to, const void* data, unsigned int size) {
    // Aloca buffer para o cabeçalho + dados
    Buffer* buf = _nic->alloc(to, protocol_number, size + sizeof(Ethernet::Header) + 14);

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

void Protocol::receive(void* buf) {
    // Conversão direta de void* para Buffer*
    Buffer* buffer = static_cast<Buffer*>(buf);

    // Endereco de destino do frame recebido
    Address dst = buffer->frame.header.dst_address;
    // Endereco de origem do frame recebido
    Address src = buffer->frame.header.src_address;

    // Tamanho do payload recebido
    unsigned int payload_size = buffer->size - sizeof(Ethernet::Header) - 14;
    
    Message message;
    message.setData(buffer->frame.payload, payload_size);

    _observed.notify(dst, message);

    // Libera o buffer alocado para o frame
    _nic->free(buffer);
}

void Protocol::attach(Concurrent_Observer* obs) {
    _observed.attach(obs);
}

void Protocol::detach(Concurrent_Observer* obs) {
    _observed.detach(obs);
}