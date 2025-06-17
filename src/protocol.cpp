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

int Protocol::send(Address from, Address to, Type type, Period period, Quadrant_ID group_id, MAC_key mac, const void* data, unsigned int size) {
    // Verifica se o destino da mensagem é interno ou externo.
    bool is_internal = false;
    if (from.vehicle_id == to.vehicle_id) {
        is_internal = true;
    }

    // Descarta envio de mensagens de interesse/resposta externas
    //   se o Veiculo ainda nao faz parte de nenhum grupo.
    if (_rsu_handler != nullptr && !is_internal &&
        !_rsu_handler->hasGroup() && type != Ethernet::TYPE_RSU_JOIN_REQ) {
        return -1;
    }

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
    payload.header.quadrant_id = group_id;     // Identificador do grupo
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

    // Preenche grupo e mac da mensagem (apenas veiculos).
    if (_rsu_handler != nullptr && !is_internal &&
        payload.header.type != Ethernet::TYPE_PTP_DELAY_REQ &&
        payload.header.type != Ethernet::TYPE_RSU_JOIN_REQ) {
        // Preenche id do grupo.
        payload.header.quadrant_id = _rsu_handler->getCurrentGroupID();
        // Preenche MAC da mensagem.
        payload.header.mac = _rsu_handler->generate_mac(payload.header, _rsu_handler->getGroupMAC(payload.header.quadrant_id));
        //std::cout << "ENVIANDO MSG COM ID DO GRUPO: " << (int)payload.header.group_id << std::endl;
    }

    std::memcpy(payload.data, data, size);  // Dados da mensagem

    // Preenche o payload do frame com os dados.
    if (!_nic->fillPayload(&buf->frame, &payload)) {
        return -1; // Retorna -1 se o preenchimento falhar
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

    // Se for RSU: descarta qual mensagem que nao seja JOIN REQ ou DELAY REQ.
    if (_rsu_handler == nullptr && 
        payload.header.type != Ethernet::TYPE_PTP_DELAY_REQ &&
        payload.header.type != Ethernet::TYPE_RSU_JOIN_REQ) {
        return;
    }

    // Descarta mensagens que não foram encaminhadas para esse veiculo.
    std::array<uint8_t, 6> mac_nulo = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    if (payload.header.dst_address.vehicle_id != mac_nulo &&
        _nic->get_address() != payload.header.dst_address.vehicle_id) {
        return;
    }

    // Descarta mensagens de interesse/respostas externas que: nao pertencem
    // ao grupo do veiculo, ou a nenhum grupo vizinho, ou que foram adulteradas.
    if (_rsu_handler != nullptr) {
        // Verifica se mensagem eh externa.
        if (payload.header.src_address.vehicle_id != _nic->get_address()) {
            // Desconsidera mensagens enviadas pela RSU.
            if (payload.header.type != Ethernet::TYPE_PTP_SYNC &&
                payload.header.type != Ethernet::TYPE_PTP_DELAY_RESP &&
                payload.header.type != Ethernet::TYPE_RSU_JOIN_RESP) {
                // Descarta mensagens de grupos que o veiculo nao pertence e nao eh vizinho.
                if (payload.header.quadrant_id != _rsu_handler->getCurrentGroupID() &&
                    !_rsu_handler->isNeighborGroup(payload.header.quadrant_id)) {
                    return;
                } else {
                    //std::cout << (int)_rsu_handler->getCurrentGroupID() << " RECEBEU INTERESSE EM POSICAO DO GRUPO: " << (int)payload.header.group_id << std::endl;
                    // Verifica MAC da mensagem.
                    if (!_rsu_handler->verify_mac(payload.header)) {
                        return; // Descarta mensagem se MAC invalido.
                    }
                    //std::cout << "MAC VALIDADO: PROCESSANDO MENSAGEM." << std::endl;
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
    message.setGroupID(payload.header.quadrant_id);         // Identificador do grupo
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

void Protocol::attach(Concurrent_Observer* obs) {
    _observed.attach(obs);
}

void Protocol::detach(Concurrent_Observer* obs) {
    _observed.detach(obs);
}
