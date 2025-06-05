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

            // Registra os tipos de mensagens que serao recebidas.
            types.push_back(Ethernet::TYPE_PTP_SYNC);
            types.push_back(Ethernet::TYPE_RSU_JOIN_RESP);
            types.push_back(Ethernet::TYPE_POSITION_DATA);

            ThreadData* thread_data = new ThreadData{this};

            int err = pthread_create(&thread, nullptr, &RSUHandler::routine, thread_data);
            if (err != 0) {
                std::cerr << "Erro ao criar thread de RSUHandler: " << strerror(err) << std::endl;
            }

            address.vehicle_id = vehicleID;
            address.component_id = thread;
        }

        ~RSUHandler() {
            running = false;
            pthread_join(thread, nullptr);
        }

        // Metodo para verificar se o veiculo pertence a um grupo.
        bool isInGroup(Ethernet::Group_ID group_id) const {
            return groups.count(group_id) > 0;
        }

        // Metodo para obter o MAC de um determinado grupo.
        Ethernet::MAC_key getGroupMAC(Ethernet::Group_ID group_id) const {
            auto it = groups.find(group_id);
            if (it != groups.end()) {
                return it->second.group_mac;
            }
            return Ethernet::MAC_key();  // Retorna uma chave MAC vazia se o grupo não for encontrado
        }
        
        // Metodo para obter o MAC do grupo atual.
        Ethernet::MAC_key getCurrentGroupMAC() const {
            if (!groups.empty()) {
                return groups.begin()->second.group_mac;  // Retorna o MAC do primeiro grupo
            }
            return Ethernet::MAC_key();  // Retorna uma chave MAC vazia se não houver grupos
        }

        // Metodo para obter o ID do grupo atual.
        Ethernet::Group_ID getCurrentGroupID() const {
            if (!groups.empty()) {
                return groups.begin()->first;  // Retorna o ID do primeiro grupo
            }
            return 0;  // Retorna 0 se não houver grupos
        }

    private:
        static void* routine(void* arg) {
            ThreadData* data = static_cast<ThreadData*>(arg);
            RSUHandler* self = data->instance;

            Communicator communicator(self->protocol, self->address.vehicle_id, pthread_self());
            self->data_publisher->subscribe(communicator.getObserver(), &self->types);

            // IMPLEMENTAR LOGICA DE ENVIO DE INTERESSE INTERNO NA POSICAO DO VEICULO.
            Ethernet::Position position;
            position.x = 0; // Exemplo de posição X
            position.y = 0; // Exemplo de posição Y

            while (self->running) {
                if (communicator.hasMessage()) {
                    Message message;
                    communicator.receive(&message);
                    switch (message.getType()) {
                        case Ethernet::TYPE_PTP_SYNC:
                        {
                            std::cout << "RSU Handler recebeu SYNC" << std::endl;

                            // Pega o quadrante da mensagem.
                            Ethernet::Quadrant quadrant = *reinterpret_cast<Ethernet::Quadrant*>(message.data());

                            std::cout << (int)message.getGroupID() << " Quadrante recebido: "
                                      << "x_min: " << quadrant.x_min << ", "
                                      << "x_max: " << quadrant.x_max << ", "
                                      << "y_min: " << quadrant.y_min << ", "
                                      << "y_max: " << quadrant.y_max << std::endl;

                            // Verifica se o Veiculo ja faz parte do grupo da RSU.
                            if (self->groups.count(message.getGroupID()) > 0) {
                                // Verifica se o Veiculo noa esta mais no quadrante do RSU.
                                if (position.x < quadrant.x_min || position.x > quadrant.x_max ||
                                    position.y < quadrant.y_min || position.y > quadrant.y_max) {
                                    // Remove o ID do grupo da estrutura de grupos que o veiculo pertence.
                                    self->groups.erase(message.getGroupID());
                                    // Atualiza lider atual.
                                    auto first_pair = self->groups.begin();
                                    self->time_sync_manager->setGrandmaster(first_pair->second.rsu_address);
                                    std::cout << "Veiculo saiu do quadrante do RSU " << (int)message.getGroupID() << ", removendo grupo." << std::endl;
                                }
                            } else { // Se o Veiculo nao faz parte do grupo da RSU.
                                std::cout << "RSU Handler recebeu SYNC de grupo desconhecido." << std::endl;
                                // Verifica se o Veiculo esta no quadrante do RSU.
                                if (position.x >= quadrant.x_min && position.x <= quadrant.x_max &&
                                    position.y >= quadrant.y_min && position.y <= quadrant.y_max) {
                                    std::cout << "Veiculo no quadrante do RSU " << (int)message.getGroupID() << ", enviando JOIN_REQ." << std::endl;
                                    // Envia mensagem de interesse em ingressar no grupo da RSU.
                                    Message joinRequest;
                                    joinRequest.setDstAddress(message.getSrcAddress());
                                    joinRequest.setType(Ethernet::TYPE_RSU_JOIN_REQ);
                                    communicator.send(&joinRequest);
                                }
                            }
                            break;
                        }   
                        case Ethernet::TYPE_RSU_JOIN_RESP:
                        {
                            std::cout << "RSU Handler recebeu JOIN_RESP" << std::endl;

                            // Pega o quadrante da mensagem.
                            Ethernet::Quadrant quadrant = *reinterpret_cast<Ethernet::Quadrant*>(message.data());

                            // Verifica se o veiculo ainda esta no quadrante do RSU.
                            if (position.x >= quadrant.x_min && position.x <= quadrant.x_max &&
                                position.y >= quadrant.y_min && position.y <= quadrant.y_max) {
                                // Adiciona o grupo na estrutura de grupos.
                                self->groups[message.getGroupID()] = {message.getSrcAddress(), message.getMAC()};
                                // Notifica TimeSyncManager sobre o novo lider.
                                self->time_sync_manager->setGrandmaster(message.getSrcAddress());
                            }
                            break;
                        }
                        case Ethernet::TYPE_POSITION_DATA:
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
        std::map<Ethernet::Group_ID, GroupData> groups;       // Map de dados dos grupos

        DataPublisher* data_publisher;                              // Publicador de dados
        TimeSyncManager* time_sync_manager;                         // Gerenciador de sincronização de tempo
        Protocol* protocol;                                         // Protocolo de comunicação
        Ethernet::Address address;                                  // Endereço do veículo (MAC + ID da thread)

        pthread_t thread;                                           // Thread de recepção de mensagens

        std::vector<Ethernet::Type> types;                          // Tipos de mensagens PTP observados
        bool running = true;
};