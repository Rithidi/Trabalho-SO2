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
            pthread_join(thread, nullptr);
        }

        // Metodo para verificar se o veiculo eh vizinho de um determinado grupo.
        bool isNeighborGroup(Ethernet::Group_ID group_id) const {
            return neighbor_groups.count(group_id) > 0;  // Retorna true se o grupo for vizinho
        }

        // Metodo para obter o MAC do grupo atual ou o MAC de um grupo vizinho.
        Ethernet::MAC_key getGroupMAC(Ethernet::Group_ID group_id) const {
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

        // Metodo para obter o ID do grupo atual.
        Ethernet::Group_ID getCurrentGroupID() const {
            return group_id;
        }

    private:
        static void* routine(void* arg) {
            ThreadData* data = static_cast<ThreadData*>(arg);
            RSUHandler* self = data->instance;

            Communicator communicator(self->protocol, self->address.vehicle_id, pthread_self());
            self->data_publisher->subscribe(communicator.getObserver(), &self->types);

            Ethernet::Position position;
            bool first_position_received = false;

            while (self->running) {
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
                            //std::cout << "RSU Handler recebeu SYNC" << std::endl;

                            // Pega o quadrante da mensagem.
                            Ethernet::Quadrant quadrant = *reinterpret_cast<Ethernet::Quadrant*>(message.data());

                            /*
                            std::cout << (int)message.getGroupID() << " Quadrante recebido: "
                                      << "x_min: " << quadrant.x_min << ", "
                                      << "x_max: " << quadrant.x_max << ", "
                                      << "y_min: " << quadrant.y_min << ", "
                                      << "y_max: " << quadrant.y_max << std::endl;
                            */

                            // Verifica se SYNC eh do proprio grupo.
                            if (self->group_id == message.getGroupID()) {
                                // Se sim, ignora SYNC.
                                //std::cout << "RSU Handler recebeu SYNC do proprio grupo." << std::endl;
                            // Verifica se SYNC eh de um grupo vizinho.
                            } else if (self->neighbor_groups.count(message.getGroupID()) > 0) {
                                // Verifica se o Veiculo saiu de perto do grupo vizinho.
                                if (position.x < quadrant.x_min - 10 || position.x > quadrant.x_max + 10 ||
                                    position.y < quadrant.y_min - 10 || position.y > quadrant.y_max + 10) {
                                    // Se sim, remove o grupo vizinho.
                                    std::cout << "Veiculo nao esta mais proximo do quadrante da RSU " << (int)message.getGroupID() << ", removendo grupo vizinho." << std::endl;
                                    self->neighbor_groups.erase(message.getGroupID());
                                }
                            // Se SYNC eh de um grupo desconhecido: Verifica se o Veiculo esta dentro ou proximo do quadrante da RSU.
                            } else {
                                std::cout << "RSU Handler recebeu SYNC de grupo desconhecido." << std::endl;
                                // Verifica se o Veiculo ja enviou JOIN_REQ para este grupo.
                                if (self->join_reqts.count(message.getGroupID()) > 0) {
                                    std::cout << "Veiculo ja enviou JOIN_REQ para o grupo " << (int)message.getGroupID() << ", ignorando SYNC." << std::endl;
                                    break; // Ignora SYNC se ja enviou JOIN_REQ.
                                }
                                // Verifica se o Veiculo esta dentro ou proximo do quadrante da RSU.
                                if (position.x >= quadrant.x_min - 10 && position.x <= quadrant.x_max + 10 &&
                                    position.y >= quadrant.y_min - 10 && position.y <= quadrant.y_max + 10) {
                                    // Se sim, envia JOIN_REQ para o grupo da RSU.
                                    std::cout << "Veiculo dentro ou proximo ao quadrante do RSU " << (int)message.getGroupID() << ", enviando JOIN_REQ." << std::endl;
                                    // Envia mensagem de interesse em ingressar no grupo da RSU.
                                    Message joinRequest;
                                    joinRequest.setDstAddress(message.getSrcAddress());
                                    joinRequest.setType(Ethernet::TYPE_RSU_JOIN_REQ);
                                    communicator.send(&joinRequest);
                                    self->join_reqts.insert(message.getGroupID());
                                }
                            }
                            break;
                        }   
                        case Ethernet::TYPE_RSU_JOIN_RESP:
                        {
                            std::cout << "RSU Handler recebeu JOIN_RESP da RSU " << (int)message.getGroupID() << std::endl;

                            // Remove o ID do grupo da lista de JOIN_REQs ja enviados.
                            self->join_reqts.erase(message.getGroupID());

                            // Pega o quadrante da mensagem.
                            Ethernet::Quadrant quadrant = *reinterpret_cast<Ethernet::Quadrant*>(message.data());

                            // Verifica se o veiculo ainda esta dentro do quadrante da RSU.
                            if (position.x >= quadrant.x_min && position.x <= quadrant.x_max &&
                                position.y >= quadrant.y_min && position.y <= quadrant.y_max) {
                                // Atualiza novo grupo do veiculo.
                                self->group_id = message.getGroupID();
                                self->group_mac = message.getMAC();
                                self->rsu_address = message.getSrcAddress();
                                // Notifica TimeSyncManager sobre o novo lider.
                                self->time_sync_manager->setGrandmaster(message.getGroupID(), message.getSrcAddress());
                                std::cout << "Veiculo ingressou no grupo da RSU " << (int)message.getGroupID() << std::endl;
                                // Verifica se o veiculo esta proximo do quadrante da RSU.
                            } else if (position.x >= quadrant.x_min - 10 && position.x <= quadrant.x_max + 10 &&
                                       position.y >= quadrant.y_min - 10 && position.y <= quadrant.y_max + 10) {
                                // Adiciona grupo a estrutura de grupos vizinhos ao veiculo.
                                self->neighbor_groups[message.getGroupID()] = {message.getSrcAddress(), message.getMAC()};
                                std::cout << "Veiculo guardou grupo RSU " << (int)message.getGroupID() <<  " como vizinho." << std::endl;
                                break;
                            } else{
                                break; // Se o veiculo nao esta dentro nem perto do quadrante, ignora JOIN_RESP.
                            }
                            break;  
                        }
                        case Ethernet::TYPE_POSITION_DATA:
                            if (!first_position_received) {
                                first_position_received = true;
                            }
                            
                            position = *reinterpret_cast<Ethernet::Position*>(message.data());
                                std::cout << "RSU Handler atualizou posicao do veiculo: "
                                          << "x: " << position.x << ", y: " << position.y << std::endl;
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

    private: 
        // Infos do grupo atual do veiculo.
        Ethernet::Group_ID group_id;
        Ethernet::MAC_key group_mac;
        Ethernet::Address rsu_address;

        // Grupos que o veiculo faz divisa.
        std::map<Ethernet::Group_ID, GroupData> neighbor_groups;

        std::set<Ethernet::Group_ID> join_reqts;         // ID dos grupos que o veiculo ja encaminhou JOIN_REQ

        DataPublisher* data_publisher;                   // Publicador de dados
        TimeSyncManager* time_sync_manager;              // Gerenciador de sincronização de tempo
        Protocol* protocol;                              // Protocolo de comunicação
        Ethernet::Address address;                       // Endereço do veículo (MAC + ID da thread)

        pthread_t thread;                                // Thread de recepção de mensagens

        std::vector<Ethernet::Type> types;               // Tipos de mensagens PTP observados
        bool running = true;

        // Intervalo de tempo para enviar mensagem de interesse nos dados do GPS.
        std::chrono::milliseconds gps_data_interval = std::chrono::milliseconds(4000);
        std::chrono::steady_clock::time_point last_gps_data_send_time = std::chrono::steady_clock::now();
};