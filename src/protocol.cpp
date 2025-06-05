#include "../include/protocol.hpp"
#include "../include/ethernet.hpp"

#include <iostream>

Protocol::Protocol(NIC<Engine>* nic, DataPublisher* data_publisher, Protocol_Number protocol_number,
                                                    RSUHandler* rsu_handler, TimeSyncManager* tsm) 
    : _nic(nic), _data_publisher(data_publisher), _time_sync_manager(tsm), protocol_number(protocol_number),
      _data_observer(this, protocol_number), _rsu_handler(rsu_handler)
{   
    _nic->attach(&_data_observer);
}

Protocol::~Protocol() {
    _nic->detach(&_data_observer);
}

int Protocol::send(Address from, Address to, Type type, Period period, Group_ID group_id, MAC_key mac, const void* data, unsigned int size) {
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
    payload.header.group_id = group_id;     // Identificador do grupo
    payload.header.mac = mac;               // MAC da mensagem

    std::chrono::system_clock::time_point now;

    // Verifica se o TimeSyncManager e RSUHandler estão disponíveis
    if (_time_sync_manager != nullptr) {
        // Se for veiculo: Obtem a hora atual de acordo com o etiquetador (TimeSyncManager).
        now = _time_sync_manager->now();
    } else {
        // Se for RSU: Obtem a hora atual do sistema.
        now = std::chrono::system_clock::now();
    }

    // Preenche o timestamp.
    payload.header.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();   // Horario de envio

    // Preenche o MAC da mensagem.
    if (_rsu_handler != nullptr && payload.header.type != Ethernet::TYPE_PTP_DELAY_REQ &&
                                   payload.header.type != Ethernet::TYPE_RSU_JOIN_REQ) {
        payload.header.mac = generate_mac(payload.header, _rsu_handler->getCurrentGroupMAC());
    }

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

    // Descarta mensagens externas de interesse ou respostas que nao pertencem ao grupo do Veiculo.
    // Verifica se o endereço de origem e destino são diferentes.
    if (payload.header.src_address.vehicle_id != payload.header.dst_address.vehicle_id) {
        // Verifica se nao eh um Veiculo e nao eh nenhuma mensagem de sincronização PTP ou resposta de ingresso RSU.
        if (_rsu_handler != nullptr && 
            payload.header.type != Ethernet::TYPE_PTP_SYNC &&
            payload.header.type != Ethernet::TYPE_PTP_DELAY_RESP &&
            payload.header.type != Ethernet::TYPE_RSU_JOIN_RESP) {
            // Verifica se a mensagem veio de algum grupo que o Veiculo nao pertence.
            if (!_rsu_handler->isInGroup(payload.header.group_id)) {
                return; // Se o Veiculo nao pertence ao grupo, descarta a mensagem
            } else {
                // Verifica o MAC da mensagem.
                if (!verify_mac(payload.header, _rsu_handler->getGroupMAC(payload.header.group_id))) {
                    return; // Se o MAC for inválido, não processa a mensagem
                }
            }
        }
    }

    // Monta mensagem com o cabeçalho e os dados recebidos.
    Message message;
    message.setSrcAddress(payload.header.src_address);   // Endereço de origem
    message.setDstAddress(payload.header.dst_address);   // Endereço de destino
    message.setType(payload.header.type);                // Tipo da mensagem
    message.setPeriod(payload.header.period);            // Período de transmissão
    message.setTimestamp(std::chrono::time_point<std::chrono::system_clock>(std::chrono::nanoseconds(payload.header.timestamp))); // Horario de envio
    message.setGroupID(payload.header.group_id);         // Identificador do grupo
    message.setMAC(payload.header.mac);                  // MAC da mensagem
    message.setData(payload.data, sizeof(payload.data)); // Copia os dados para a mensagem

    // Encaminha mensagens de interesse direto para o DataPublisher.
    if (pthread_equal(payload.header.dst_address.component_id, (pthread_t)0)) {
        _data_publisher->notify(message);
    } else {
        // Notifica os observadores com o endereço de destino e a mensagem
        _observed.notify(message);
    }
}

// Gera o MAC (Message Authentication Code) com o cabeçalho da mensagem usando a chave do grupo
Ethernet::MAC_key Protocol::generate_mac(const Ethernet::Header& header, const Ethernet::MAC_key& group_key) {
    Ethernet::MAC_key mac;

    const unsigned char* data = reinterpret_cast<const unsigned char*>(&header);
    size_t header_size = sizeof(Ethernet::Header) - sizeof(Ethernet::MAC_key); // exclui o campo MAC

    // Inicializa mac com a chave do grupo
    std::memcpy(mac.data(), group_key.data(), 16);

    // XOR byte a byte com os bytes do header (exceto MAC)
    for (size_t i = 0; i < header_size; ++i) {
        mac[i % 16] ^= data[i];
    }

    return mac;
}

// Verifica se o MAC da mensagem corresponde ao esperado
bool Protocol::verify_mac(const Ethernet::Header& header, const Ethernet::MAC_key& group_key) {
    Ethernet::MAC_key expected_mac = generate_mac(header, group_key);
    return expected_mac == header.mac;
}

void Protocol::attach(Concurrent_Observer* obs) {
    _observed.attach(obs);
}

void Protocol::detach(Concurrent_Observer* obs) {
    _observed.detach(obs);
}
