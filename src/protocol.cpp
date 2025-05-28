#include "../include/protocol.hpp"
#include "../include/ethernet.hpp"

#include <iostream>

Protocol::Protocol(NIC<Engine>* nic, DataPublisher* data_publisher, Protocol_Number protocol_number) 
    : _nic(nic), _data_publisher(data_publisher), _time_sync_manager(_data_publisher, this, _nic->get_address()),
     protocol_number(protocol_number), _data_observer(this, protocol_number)
{   
    _nic->attach(&_data_observer);
};

Protocol::~Protocol() {
    _nic->detach(&_data_observer);
};

int Protocol::send(Address from, Address to, Type type, Period period, const void* data, unsigned int size) {
    // Pede para a NIC alocar um buffer para o frame Ethernet
    Buffer* buf = _nic->alloc();

    // Verifica se o buffer foi alocado corretamente
    if (buf == nullptr) return -1;

    // Preenche o cabeçalho do frame Ethernet
    buf->frame.src = from.vehicle_id; // Endereço de origem
    buf->frame.type = htons(0x88B5);   // Tipo do protocolo (customizado)

    // Monta Payload com o cabeçalho e a mensagem
    Ethernet::Payload payload;
    payload.header.src_address = from;      // Endereço de origem
    payload.header.dst_address = to;        // Endereço de destino
    payload.header.type = type;             // Tipo da mensagem
    payload.header.period = period;         // Período de transmissão

    // Obtem a hora atual de acordo com o etiquetador (TimeSyncManager).
    auto now = _time_sync_manager.now();
    auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    payload.header.timestamp = timestamp;   // Horario de envio

    std::memcpy(payload.data, data, size);  // Dados da mensagem

    // Preenche o payload do frame com os dados.
    if (!_nic->fillPayload(&buf->frame, &payload)) {
        return -1; // Retorna -1 se o preenchimento falhar
    }   

    bool is_internal = false;

    // Verifica se o endereço MAC de origem e destino são iguais
    if (from.vehicle_id == to.vehicle_id) {
        is_internal = true; // Define como interno se os endereços forem iguais
    }
    
    // Envia o frame Ethernet para a NIC
    return _nic->send(buf, is_internal);
}

void Protocol::receive(void* buf) {
    // Conversão direta de void* para Buffer*
    Buffer* buffer = static_cast<Buffer*>(buf);

    // Inicializa Payload para receber os dados.
    Ethernet::Payload payload;

    // Extrai o payload do frame recebido.
    _nic->extractPayload(&buffer->frame, &payload);

    // Libera o buffer após o uso
    delete buffer;

    // Monta mensagem com o cabeçalho e os dados recebidos.
    Message message;
    message.setSrcAddress(payload.header.src_address);   // Endereço de origem
    message.setDstAddress(payload.header.dst_address);   // Endereço de destino
    message.setType(payload.header.type);                // Tipo da mensagem
    message.setPeriod(payload.header.period);            // Período de transmissão
    message.setTimestamp(std::chrono::time_point<std::chrono::system_clock>(std::chrono::nanoseconds(payload.header.timestamp))); // Horario de envio
    message.setData(payload.data, sizeof(payload.data)); // Copia os dados para a mensagem

    // Encaminha mensagens de interesse direto para o DataPublisher.
    if (pthread_equal(payload.header.dst_address.component_id, (pthread_t)0)) {
        _data_publisher->notify(message);
    } else {
        // Notifica os observadores com o endereço de destino e a mensagem
        _observed.notify(message);
    }

    // Notifica os observadores com o endereço de destino e a mensagem
    //_observed.notify(message);
}


void Protocol::attach(Concurrent_Observer* obs) {
    _observed.attach(obs);
}

void Protocol::detach(Concurrent_Observer* obs) {
    _observed.detach(obs);
}
