#pragma once

#include <chrono>
#include <pthread.h>
#include <iostream>
#include <random>
#include <cstdint>
#include <string>
#include <array>
#include <sstream>
#include <iomanip>

#include "../include/communicator.hpp"
#include "../include/message.hpp"
#include "../include/nic.hpp"
#include "../include/protocol.hpp"
#include "../include/engine.hpp"
#include "../include/data_publisher.hpp"
#include "../include/ethernet.hpp"

class RSU {
    public:
        struct ThreadData {
            RSU* instance;
        };
        
        // Construtor.
        RSU(const std::string& interface, Ethernet::Group_ID groupId, Ethernet::Quadrant quadt) : nic(interface), 
                protocol(&nic, &data_publisher, 0x88B5), group_id(groupId), quadrant(quadt), running(true) {
            
            // Inicializa o MAC do grupo.
            mac = generate_group_key();

            types.push_back(Ethernet::TYPE_PTP_DELAY_REQ);
            types.push_back(Ethernet::TYPE_RSU_JOIN_REQ);

            std::cout << "RSU criada com ID do grupo: " << (int)group_id << " e MAC: " << mac_key_to_string(mac) << std::endl;

            // Cria e inicia a thread de recebimento.
            thread_data = new ThreadData{this};
            int err = pthread_create(&thread, nullptr, &RSU::receive_routine, thread_data);
            if (err != 0) {
                std::cerr << "Erro ao criar thread de recepção: " << strerror(err) << std::endl;
            } else {
                // Aguarda até que a thread de recepção inicialize o communicator
                std::unique_lock<std::mutex> lock(mutex);
                cv.wait(lock, [&] { return communicator_ready; });
            }
            // Cria thread periodica de envio.
            err = pthread_create(&periodic_thread, nullptr, &RSU::send_routine, thread_data);
            if (err != 0) {
                std::cerr << "Erro ao criar thread de envio: " << strerror(err) << std::endl;
            }
        }
        // Destrutor.
        ~RSU() {
            running = false;
            cv.notify_all();
            pthread_join(thread, nullptr);
            pthread_join(periodic_thread, nullptr);
            delete communicator;
            delete thread_data;
        }
    
    private:
        // Funcao de rotina da thread periodica para envio da mensagem SYNC.
        static void* send_routine(void* arg) {
            ThreadData* data = static_cast<ThreadData*>(arg);
            RSU* self = data->instance;

            auto next_send = std::chrono::system_clock::now();

            std::unique_lock<std::mutex> lock(self->mutex);
            while (self->running) {
                next_send += std::chrono::milliseconds(100);

                // Envia PTP_SYNC junto com ID e Quadrante da RSU.
                Message message;
                message.setDstAddress({{0,0,0,0,0,0}, (pthread_t)0});
                message.setType(Ethernet::TYPE_PTP_SYNC);
                message.setGroupID(self->group_id);
                //std::cout << "RSU " << (int)message.getGroupID() << " enviando SYNC" << std::endl;
                message.setData(reinterpret_cast<Ethernet::Quadrant*>(&self->quadrant), sizeof(Ethernet::Quadrant));
                message.setPeriod(0);
                //std::cout << "RSU " << (int)self->group_id << " enviou SYNC" << std::endl;
                self->communicator->send(&message);

                self->cv.wait_until(lock, next_send, [&] { return !self->running; });
            }
            pthread_exit(nullptr);
        }

        // Funcao de rotina da thread de recepcao de mensagens JOIN e DELAY_REQ.
        static void* receive_routine(void* arg) {
            ThreadData* data = static_cast<ThreadData*>(arg);
            RSU* self = data->instance;

            self->communicator = new Communicator(&self->protocol, self->nic.get_address(), pthread_self());
            // Sinaliza que o communicator foi inicializado
            {
                std::lock_guard<std::mutex> lock(self->mutex);
                self->communicator_ready = true;
            }
            self->cv.notify_all(); // Acorda thread principal para continuar
            self->data_publisher.subscribe(self->communicator->getObserver(), &self->types);

            while (self->running) {
                //std::cout << "RSU aguardando mensagens..." << std::endl;
                if (self->communicator->hasMessage()) {
                    Message message;
                    self->communicator->receive(&message);
                    switch (message.getType()) {
                        case Ethernet::TYPE_RSU_JOIN_REQ:
                            // Responde veiculo com ID, MAC e Quadrante do grupo (RSU).
                            //std::cout << "RSU " << (int)self->group_id << " recebeu JOIN_REQ" << std::endl;
                            message.setType(Ethernet::TYPE_RSU_JOIN_RESP);
                            message.setDstAddress(message.getSrcAddress());
                            message.setGroupID(self->group_id);
                            message.setMAC(self->mac);
                            message.setPeriod(0);
                            message.setData(reinterpret_cast<Ethernet::Quadrant*>(&self->quadrant), sizeof(Ethernet::Quadrant));
                            //std::cout << "RSU " << (int)self->group_id << " enviou JOIN_RESP" << std::endl;
                            self->communicator->send(&message);
                            break;
                        case Ethernet::TYPE_PTP_DELAY_REQ:
                            // Responde veiculo com DELAY RESP.
                            //std::cout << "RSU " << (int)self->group_id << " recebeu DELAY_REQ" << std::endl;
                            message.setType(Ethernet::TYPE_PTP_DELAY_RESP);
                            message.setDstAddress(message.getSrcAddress());
                            message.setPeriod(0);
                            //std::cout << "RSU " << (int)self->group_id << " enviou DELAY_RESP" << std::endl;
                            self->communicator->send(&message);
                            break;
                        default:
                            break;
                    }
                }
            }
            self->data_publisher.unsubscribe(self->communicator->getObserver());
            pthread_exit(nullptr);
        }

        // Gera uma chave MAC aleatória para o grupo.
        Ethernet::MAC_key generate_group_key() {
            Ethernet::MAC_key key;
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<uint16_t> dist(0, 255);

            for (auto& byte : key) {
                byte = static_cast<uint8_t>(dist(gen));
            }
            return key;
        }

        std::string mac_key_to_string(const Ethernet::MAC_key& mac) {
            std::ostringstream oss;
            for (size_t i = 0; i < mac.size(); ++i) {
                oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(mac[i]);
                if (i != mac.size() - 1) {
                    oss << ":";
                }
            }
            return oss.str();
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
        NIC<Engine> nic;            
        Protocol protocol;     
        DataPublisher data_publisher;
        Communicator* communicator;

        bool communicator_ready = false;

        pthread_t thread;
        pthread_t periodic_thread;
        ThreadData* thread_data;

        std::condition_variable cv;
        std::mutex mutex;

        std::vector<Ethernet::Type> types;

        Ethernet::Group_ID group_id;
        Ethernet::Quadrant quadrant;
        Ethernet::MAC_key mac;
        bool running;
};