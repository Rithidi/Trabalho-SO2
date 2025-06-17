#pragma once

#include "../include/ethernet.hpp"
#include "../include/protocol.hpp"
#include "../include/data_publisher.hpp"
#include "../include/message.hpp"

#include <pthread.h>
#include <iostream>
#include <map>

class RSUHandler {
    public:
        struct GroupData {
            Ethernet::Address rsu_address;  // Endereço da RSU
            Ethernet::MAC_key group_mac;    // MAC do grupo
        };

        // Estrutura usada para passar o this para a thread
        struct ThreadData {
            RSUHandler* instance;
        };

        // Construtor.
        RSUHandler(DataPublisher* dataPublisher, TimeSyncManager* timeSyncManager, Protocol* protocol, Ethernet::Mac_Address vehicleID)
            : data_publisher(dataPublisher), time_sync_manager(timeSyncManager), protocol(protocol) {

            address.vehicle_id = vehicleID;

            // Registra os tipos de mensagens de interesse que serao recebidas.
            types.push_back(Ethernet::TYPE_PTP_SYNC);

            ThreadData* thread_data = new ThreadData{this};

            int err = pthread_create(&thread, nullptr, &RSUHandler::routine, thread_data);
            if (err != 0) {
                std::cerr << "Erro ao criar thread de RSUHandler: " << strerror(err) << std::endl;
            }

            address.component_id = thread;
        }

        ~RSUHandler() {
            running = false;
        }

        // Metodo para verificar se o veiculo eh vizinho de um determinado grupo.
        bool isNeighborGroup(Ethernet::Quadrant_ID group_id) const {
            return neighbor_groups.count(group_id) > 0;  // Retorna true se o grupo for vizinho
        }

        // Metodo para obter o MAC do grupo atual ou o MAC de um grupo vizinho.
        Ethernet::MAC_key getGroupMAC(Ethernet::Quadrant_ID group_id) const {
            // Verifica se o grupo eh vizinho.
            if (neighbor_groups.count(group_id) > 0) {
                return neighbor_groups.at(group_id).group_mac;  // Retorna o MAC do grupo vizinho
            }
            // Se nao for vizinho, verifica se eh o grupo atual.
            if (group_id == this->group_id) {
                return group_mac;  // Retorna o MAC do grupo atual
            }
            // Se nao for vizinho nem o grupo atual, retorna uma chave MAC vazia.
            return Ethernet::MAC_key();  // Retorna uma chave MAC vazia
        }

        // Gera MAC usando o cabeçalho da mensagem (exeto campo mac) e a chave doo grupo.
        Ethernet::MAC_key generate_mac(const Ethernet::Header& header, const Ethernet::MAC_key group_key) {
            // Faz XOR dos bytes do cabeçalho (exeto mac) com a chave do grupo.
            Ethernet::MAC_key mac;

            // Copia a chave do grupo como base do MAC
            std::memcpy(mac.data(), group_key.data(), 16);

            // Obtém ponteiro para o início do header
            const uint8_t* data = reinterpret_cast<const uint8_t*>(&header);

            // Calcula o tamanho do cabeçalho sem o campo MAC (últimos 17 bytes: 16 de MAC + 1 de group_id)
            constexpr size_t mac_offset = offsetof(Ethernet::Header, mac);
            constexpr size_t mac_field_size = sizeof(Ethernet::MAC_key) + sizeof(Ethernet::Quadrant_ID);
            const size_t size_to_hash = sizeof(Ethernet::Header) - mac_field_size;

            // Faz XOR do conteúdo do header com os 16 bytes do MAC base
            for (size_t i = 0; i < size_to_hash; ++i) {
                mac[i % 16] ^= data[i];
            }

            return mac;
        }

        // Verifica MAC da mensagem para cada chave de grupo (pertencente e vizinhos).
        bool verify_mac(const Ethernet::Header& header) {
            // Verifica MAC com chave do grupo que o veiculo pertence.
            if (header.mac == generate_mac(header, group_mac)) {
                return true;
            }
            // Verifica MAC com chave de cada grupo vizinho.
            for (const auto& [groupId, groupData] : neighbor_groups) {
                if (header.mac == generate_mac(header, groupData.group_mac)) {
                    return true;
                }
            }
            return false;
        }

        // Metodo para obter o ID do grupo atual.
        Ethernet::Quadrant_ID getCurrentGroupID() const {
            return group_id;
        }

        // Metodo para verificar se Veiculo faz parte de algum grupo. 
        bool hasGroup() {
            return has_group;
        }

    private:
        static void* routine(void* arg) {
            ThreadData* data = static_cast<ThreadData*>(arg);
            RSUHandler* self = data->instance;

            Communicator communicator(self->protocol, self->address.vehicle_id, pthread_self());
            self->data_publisher->subscribe(communicator.getObserver(), &self->types);

            Ethernet::Position position;
            bool first_position_received = false;

            while (true) {
                // Verifica se o intervalo de tempo para enviar dados do GPS foi atingido.
                if (std::chrono::steady_clock::now() - self->last_gps_data_send_time >= self->gps_data_interval) {
                    // Envia mensagem de interesse nos dados do GPS.
                    Message message;
                    message.setDstAddress({self->address.vehicle_id, (pthread_t)0});
                    message.setPeriod(0);
                    message.setType(Ethernet::TYPE_POSITION_DATA);
                    communicator.send(&message);
                    self->last_gps_data_send_time = std::chrono::steady_clock::now();
                }

                if (communicator.hasMessage()) {
                    Message message;
                    communicator.receive(&message);
                    if (message.getType() == Ethernet::TYPE_POSITION_DATA && !first_position_received) {
                        first_position_received = true;
                    }
                    if (!first_position_received) {
                        continue;  // Ignora mensagens se a primeira posicao ainda nao foi recebida.
                    }
                    switch (message.getType()) {
                        case Ethernet::TYPE_PTP_SYNC:
                        {
                            // Extrai o quadrante da mensagem SYNC.
                            Ethernet::Quadrant quadrant = *reinterpret_cast<Ethernet::Quadrant*>(message.data());
                            uint8_t group_id = message.getGroupID();

                            // Se a mensagem for do próprio grupo, ignora.
                            if (self->has_group && self->group_id == group_id) {
                                break;
                            }

                            // Verifica se o veículo está dentro ou próximo do quadrante.
                            bool in_quadrant = position.x >= quadrant.x_min && position.x <= quadrant.x_max &&
                                            position.y >= quadrant.y_min && position.y <= quadrant.y_max;

                            bool near_quadrant = position.x >= quadrant.x_min - 10 && position.x <= quadrant.x_max + 10 &&
                                                position.y >= quadrant.y_min - 10 && position.y <= quadrant.y_max + 10;

                            // Se a SYNC veio de um grupo vizinho conhecido:
                            if (self->neighbor_groups.count(group_id)) {
                                // Se o veículo se afastou do quadrante do grupo vizinho, remove dos vizinhos.
                                if (!near_quadrant) {
                                    self->neighbor_groups.erase(group_id); // Remove grupo da estrutura de grupos vizinhos.
                                    // Remove threads periodicas do DataPublisher destinadas ao antigo grupo vizinho.
                                    self->data_publisher->delete_group_threads(self->group_id);
                                }
                                // Se o veículo entrou no quadrante, envia JOIN_REQ e remove dos vizinhos.
                                else if (in_quadrant) {
                                    self->neighbor_groups.erase(group_id);

                                    self->print_address(self->address.vehicle_id);
                                    std::cout << " Veiculo dentro ou proximo ao quadrante do RSU " << (int)group_id << ", enviando JOIN_REQ." << std::endl;

                                    Message joinRequest;
                                    joinRequest.setDstAddress(message.getSrcAddress());
                                    joinRequest.setType(Ethernet::TYPE_RSU_JOIN_REQ);
                                    communicator.send(&joinRequest);
                                    self->join_reqts.insert(group_id);
                                }
                            }
                            // Se a SYNC veio de um grupo desconhecido:
                            else {
                                // Se já foi enviado um JOIN_REQ anteriormente, ignora.
                                if (self->join_reqts.count(group_id)) {
                                    self->print_address(self->address.vehicle_id);
                                    std::cout << " Veiculo ja enviou JOIN_REQ para o grupo " << (int)group_id << ", ignorando SYNC." << std::endl;
                                    break;
                                }

                                // Se estiver dentro ou próximo do quadrante, envia JOIN_REQ.
                                if (near_quadrant) {
                                    self->print_address(self->address.vehicle_id);
                                    std::cout << " Veiculo dentro ou proximo ao quadrante do RSU " << (int)group_id << ", enviando JOIN_REQ." << std::endl;

                                    Message joinRequest;
                                    joinRequest.setDstAddress(message.getSrcAddress());
                                    joinRequest.setType(Ethernet::TYPE_RSU_JOIN_REQ);
                                    communicator.send(&joinRequest);
                                    self->join_reqts.insert(group_id);
                                }
                            }

                            break;
                        }
                        case Ethernet::TYPE_RSU_JOIN_RESP:
                        {   
                            self->print_address(message.getDstAddress().vehicle_id);
                            std::cout <<" RSU Handler recebeu JOIN_RESP da RSU " << (int)message.getGroupID() << std::endl;

                            // Remove o ID do grupo da lista de JOIN_REQs ja enviados.
                            self->join_reqts.erase(message.getGroupID());

                            // Pega o quadrante da mensagem.
                            Ethernet::Quadrant quadrant = *reinterpret_cast<Ethernet::Quadrant*>(message.data());

                            // Verifica se o veículo está dentro ou próximo do quadrante.
                            bool in_quadrant = position.x >= quadrant.x_min && position.x <= quadrant.x_max &&
                                            position.y >= quadrant.y_min && position.y <= quadrant.y_max;

                            bool near_quadrant = position.x >= quadrant.x_min - 10 && position.x <= quadrant.x_max + 10 &&
                                                position.y >= quadrant.y_min - 10 && position.y <= quadrant.y_max + 10;

                            // Verifica se o veiculo ainda esta dentro do quadrante da RSU.
                            if (in_quadrant) {
                                if (!self->has_group) {
                                    self->has_group = true;
                                } else {
                                    // Remove threads periodicas do DataPublisher destinadas ao grupo antigo.
                                    self->data_publisher->delete_group_threads(self->group_id);
                                }
                                // Atualiza novo grupo do veiculo.
                                self->group_id = message.getGroupID();
                                self->group_mac = message.getMAC();
                                self->rsu_address = message.getSrcAddress();

                                self->print_address(self->address.vehicle_id);
                                std::cout << " ingressou no grupo da RSU " << (int)message.getGroupID() << std::endl;

                                // Notifica TimeSyncManager sobre o novo lider.
                                self->time_sync_manager->setGrandmaster(message.getGroupID(), message.getSrcAddress());
                                
                                // Verifica se o veiculo esta proximo do quadrante da RSU.
                            } else if (near_quadrant) {
                                // Adiciona grupo a estrutura de grupos vizinhos ao veiculo.
                                self->neighbor_groups[message.getGroupID()] = {message.getSrcAddress(), message.getMAC()};

                                self->print_address(self->address.vehicle_id);
                                std::cout << " vizinho ao grupo da RSU " << (int)message.getGroupID() << std::endl;
                                 break;
                            } else{
                                break; // Se o veiculo nao esta dentro nem perto do quadrante, ignora JOIN_RESP.
                            }
                            break;  
                        }
                        case Ethernet::TYPE_POSITION_DATA:
                            if (!first_position_received) { first_position_received = true; }
                            position = *reinterpret_cast<Ethernet::Position*>(message.data());
                            //std::cout << "RSU Handler recebeu nova posicao do veiculo: "<< "x: " << position.x << ", y: " << position.y << std::endl;
                            break;
                        default:
                            break;
                    }
                }
            }
            self->data_publisher->unsubscribe(communicator.getObserver());
            delete data;
            pthread_exit(NULL);
        }

        // Exibe endereço MAC formatado
        void print_address(const Ethernet::Mac_Address& vehicle_id) {
            std::cout << "Vehicle ID: ";
            for (size_t i = 0; i < vehicle_id.size(); ++i) {
                std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(vehicle_id[i]);
                if (i != vehicle_id.size() - 1) std::cout << ":";
            }
            std::cout << std::dec;
        }

    private:
        // Variavel de status de grupo.
        bool has_group = false;

        // Infos do grupo atual do veiculo.
        Ethernet::Quadrant_ID group_id;
        Ethernet::MAC_key group_mac;
        Ethernet::Address rsu_address;

        // Grupos que o veiculo faz divisa.
        std::map<Ethernet::Quadrant_ID, GroupData> neighbor_groups;

        std::set<Ethernet::Quadrant_ID> join_reqts;         // ID dos grupos que o veiculo ja encaminhou JOIN_REQ

        DataPublisher* data_publisher;                   // Publicador de dados
        TimeSyncManager* time_sync_manager;              // Gerenciador de sincronização de tempo
        Protocol* protocol;                              // Protocolo de comunicação
        Ethernet::Address address;                       // Endereço do veículo (MAC + ID da thread)

        pthread_t thread;                                // Thread de recepção de mensagens

        std::vector<Ethernet::Type> types;               // Tipos de mensagens PTP observados
        bool running = true;

        // Intervalo de tempo para enviar mensagem de interesse nos dados do GPS.
        std::chrono::milliseconds gps_data_interval = std::chrono::milliseconds(100);
        std::chrono::steady_clock::time_point last_gps_data_send_time = std::chrono::steady_clock::now();
};