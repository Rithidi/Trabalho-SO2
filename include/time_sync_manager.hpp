#pragma once

#include <pthread.h>
#include <set>
#include <vector>
#include <chrono>
#include <tuple>
#include <iostream>
#include <iomanip>
#include <random>
#include <iomanip>  // std::setfill, std::setw, std::setprecision
#include <sstream>  // std::ostringstream

#include "../include/communicator.hpp"
#include "../include/ethernet.hpp"
#include "../include/protocol.hpp"
#include "../include/data_publisher.hpp"
#include "../include/message.hpp"

// classe responsável por gerenciar a sincronização de tempo entre veiculos.
class TimeSyncManager {
public:
    using Clock = std::chrono::system_clock;

    // Estrutura usada para passar o this para a thread
    struct ThreadData {
        TimeSyncManager* instance;
    };

    // Construtor: inicializa variáveis, registra os tipos de mensagens PTP e inicia a thread de sincronização
    TimeSyncManager(DataPublisher* dataPublisher, Protocol* protocol, Ethernet::Mac_Address vehicleID)
        : dataPublisher(dataPublisher), protocol(protocol), running(true) {

        print_address(vehicleID);
        std::cout << " | Offset inicial do relógio: " << defaultClockOffset.count() << " µs\n" << std::endl;
        
        // Tipos de mensagens PTP que serão recebidas.
        types.push_back(Ethernet::TYPE_PTP_SYNC);
        types.push_back(Ethernet::TYPE_PTP_DELAY_RESP);

        // Cria e inicia a thread de sincronização
        ThreadData* data = new ThreadData{this};
        int err = pthread_create(&thread, nullptr, &TimeSyncManager::time_sync_routine, data);
        if (err != 0) {
            delete data;
            std::cerr << "Erro ao criar thread de sincronização: " << strerror(err) << std::endl;
        }

        address.vehicle_id = vehicleID;
        address.component_id = thread;
    }

    // Destrutor: encerra a thread
    ~TimeSyncManager() {
        running = false;
    }

    // Retorna o horário atual corrigido pelo offset calculado, em microssegundos
    Clock::time_point now() {
        return Clock::now() + std::chrono::duration_cast<Clock::duration>(defaultClockOffset) +
                              std::chrono::duration_cast<Clock::duration>(clockOffset);
    }

    void setGrandmaster(Ethernet::Group_ID groupId, const Ethernet::Address& address) {
        //clockOffset = std::chrono::microseconds(0); // Reseta o clockOffset ao definir o Grandmaster
        grandmasterAddress = address;            // Define o endereço do Grandmaster
        grandmasterGroupId = groupId;            // Define o ID do grupo do Grandmaster
    }

private:
    // Função de rotina da thread de sincronização temporal.
    static void* time_sync_routine(void* arg) {
        ThreadData* data = static_cast<ThreadData*>(arg);
        TimeSyncManager* self = data->instance;

        Communicator communicator(self->protocol, self->address.vehicle_id, pthread_self());
        self->dataPublisher->subscribe(communicator.getObserver(), &self->types);

        while (self->running) {
            if (communicator.hasMessage()) {
                Message message;
                communicator.receive(&message);
                switch (message.getType()) {
                    case Ethernet::TYPE_PTP_SYNC:
                        // Verifica se o remetente é o Grandmaster
                        if (message.getGroupID() == self->grandmasterGroupId) {
                            std::cout << "⏱️  Sincronizando tempo com a RSU " << (int)self->grandmasterGroupId << std::endl;
                            self->syncSendTime = message.getTimestamp();
                            self->syncRecvTime = self->now();

                            // Envia mensagem de Delay Request para o Grandmaster.
                            message.setType(Ethernet::TYPE_PTP_DELAY_REQ);
                            message.setDstAddress(self->grandmasterAddress);
                            message.setPeriod(0);
                            communicator.send(&message);

                            self->delayReqSendTime = message.getTimestamp();
                        }
                        break;
                    case Ethernet::TYPE_PTP_DELAY_RESP:
                    {
                        //std::cout << "Delay RESP recebido" << std::endl;
                        self->delayRespRecvTime = message.getTimestamp();
                        // Cálculo do offset baseado nas fórmulas do PTP
                        // t1: syncSendTime (GM)       t2: syncRecvTime (Slave)
                        // t3: delayReqSendTime (Slave) t4: delayRespRecvTime (GM)
                        
                        auto t1 = self->syncSendTime;
                        auto t2 = self->syncRecvTime;
                        auto t3 = self->delayReqSendTime;
                        auto t4 = self->delayRespRecvTime;

                        // Calcula offset e delay corretamente
                        //auto offset = std::chrono::duration_cast<std::chrono::microseconds>(((t2 - t1) - (t4 - t3)) / 2);
                        //auto delay  = std::chrono::duration_cast<std::chrono::microseconds>((t2 - t1 + (t4 - t3)) / 2);

                        // Atualiza o clockOffset com o novo offset calculado
                        self->clockOffset = std::chrono::duration_cast<std::chrono::microseconds>(((t2 - t1) - (t4 - t3)) / 2);

                        /*
                        std::cout << "\n(S) Offset ajustado: " << self->clockOffset.count() << " µs (";
                        self->print_address(self->address.vehicle_id);
                        std::cout << ")" << std::endl;
                        */

                        //std::cout << "Delay: " << delay.count() << " µs" << std::endl;
                        //std::cout << "syncRecvTime: " << std::chrono::duration_cast<std::chrono::microseconds>(syncRecvTime.time_since_epoch()).count() << " µs" << std::endl;
                        //std::cout << "syncSendTime: " << std::chrono::duration_cast<std::chrono::microseconds>(syncSendTime.time_since_epoch()).count() << " µs" << std::endl;
                        //std::cout << "delayRespRecvTime: " << std::chrono::duration_cast<std::chrono::microseconds>(delayRespRecvTime.time_since_epoch()).count() << " µs" << std::endl;
                        //std::cout << "delayReqSendTime: " << std::chrono::duration_cast<std::chrono::microseconds>(delayReqSendTime.time_since_epoch()).count() << " µs\n" << std::endl;
                        break;
                    }
                    default:
                        break;
                }
            }
        }
        self->dataPublisher->unsubscribe(communicator.getObserver());
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

    // Exibe o horário UTC formatado com microssegundos
    void print_time_utc() {
        auto time_now = now();
        std::time_t time_t_now = std::chrono::system_clock::to_time_t(time_now);
        std::tm* gmt = std::gmtime(&time_t_now);

        auto duration = time_now.time_since_epoch();
        auto micros = std::chrono::duration_cast<std::chrono::microseconds>(duration).count() % 1000000;

        std::cout << "Relogio atual: "
                  << std::put_time(gmt, "%Y-%m-%d %H:%M:%S")
                  << "." << std::setfill('0') << std::setw(6) << micros
                  << " UTC" << std::endl;
    }

private:
    // Dependências externas
    DataPublisher* dataPublisher;
    Protocol* protocol;

    pthread_t thread;
    Ethernet::Address address;

    // Informações do Grandmaster
    Ethernet::Address grandmasterAddress;
    Ethernet::Group_ID grandmasterGroupId;

    std::vector<Ethernet::Type> types;  // Tipos de mensagens PTP observados
    bool running = true;

    // Marcas de tempo usadas nos cálculos PTP
    Clock::time_point syncSendTime;
    Clock::time_point syncRecvTime;
    Clock::time_point delayReqSendTime;
    Clock::time_point delayRespRecvTime;
    
    // Offset e gerador aleatório de offset inicial
    std::random_device rd;
    std::mt19937 gen{rd()};
    std::uniform_int_distribution<int64_t> dist{0, 10000};

    // Offset default do veiculo (utilizado para simular erro de relogio).
    std::chrono::microseconds defaultClockOffset{dist(gen)};
    // Offset do GM atual.
    std::chrono::microseconds clockOffset{0};
};
