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
    buf->frame.src = from.mac_address; // Endereço de origem
    buf->frame.type = htons(0x88B5);   // Tipo do protocolo (customizado)

    // Inicializa o cabeçalho e a mensagem
    Ethernet::Header header;
    Message message;

    // Preenche o cabeçalho com os endereços de origem e destino
    header.src_address = from;  // Endereço de origem
    header.dst_address = to;    // Endereço de destino
    message.setData(data, size); // Dados da mensagem

    // Verifica se o tamanho do payload não excede o máximo permitido
    if (!_nic->fillPayload(&buf->frame, &header, &message)) {
        return -1; // Retorna -1 se o preenchimento falhar
    }

    // Envia o frame pela NIC
    return _nic->send(buf);
}

void Protocol::receive(void* buf) {
    // Conversão direta de void* para Buffer*
    Buffer* buffer = static_cast<Buffer*>(buf);

    // Inicializa o cabeçalho e a mensagem
    Ethernet::Header header;
    Message message;

    // Chama o método extractPayload para preencher o cabeçalho e a mensagem
    _nic->extractPayload(&buffer->frame, &header, &message);

    // Obtém os endereços de origem e destino do cabeçalho
    Address dst = header.dst_address; // Endereço de destino
    Address src = header.src_address; // Endereço de origem

    // Notifica os observadores com o endereço de destino e a mensagem
    _observed.notify(dst, message);

    // Libera o buffer alocado para o frame
    //_nic->free(buffer);
}


void Protocol::attach(Concurrent_Observer* obs) {
    _observed.attach(obs);
}

void Protocol::detach(Concurrent_Observer* obs) {
    _observed.detach(obs);
}
