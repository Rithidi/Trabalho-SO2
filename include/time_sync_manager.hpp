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

// lasse responsável por gerenciar a sincronização de tempo entre veiculos.
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
        
        // Tipos de mensagens do PTP que serão processadas
        types.push_back(Ethernet::TYPE_PTP_ANNOUNCE);
        types.push_back(Ethernet::TYPE_PTP_SYNC);
        types.push_back(Ethernet::TYPE_PTP_DELAY_REQ);
        types.push_back(Ethernet::TYPE_PTP_DELAY_RESP);

        // Cria e inicia a thread de sincronização
        ThreadData* data = new ThreadData{this};
        int err = pthread_create(&thread, nullptr, &TimeSyncManager::time_sync_routine, data);
        if (err != 0) {
            std::cerr << "Erro ao criar thread: " << strerror(err) << std::endl;
        }

        address.vehicle_id = vehicleID;
        address.component_id = thread;
    }

    // Destrutor: encerra a thread
    ~TimeSyncManager() {
        running = false;
        pthread_join(thread, nullptr);
    }

    // Retorna o horário atual corrigido pelo offset calculado, em microssegundos
    Clock::time_point now() {
        return Clock::now() + std::chrono::duration_cast<Clock::duration>(defaultClockOffset) +
                              std::chrono::duration_cast<Clock::duration>(clockOffset);
    }

private:
    // Função de rotina da thread de sincronização temporal.
    static void* time_sync_routine(void* arg) {
        ThreadData* data = static_cast<ThreadData*>(arg);
        TimeSyncManager* self = data->instance;

        Communicator communicator(self->protocol, self->address.vehicle_id, pthread_self());
        self->dataPublisher->subscribe(communicator.getObserver(), &self->types);

        self->sendAnnounce(communicator);
        self->lastAnnounceTime = Clock::now();

        while (self->running) {
            if (communicator.hasMessage()) {
                Message msg;
                communicator.receive(&msg);
                self->processPTPMessage(&msg, &communicator);
            }

            auto now = Clock::now();

            // Envia mensagem de sincronização periodicamente (apenas se for GM)
            if (self->isGrandmaster && (now - self->lastSyncTime >= std::chrono::duration_cast<Clock::duration>(std::chrono::microseconds(self->syncInterval.count() * 1000)))) {
                self->sendSync(communicator);
                self->lastSyncTime = now;
            }

            // Reenvia anúncio periodicamente
            if (now - self->lastAnnounceTime >= std::chrono::duration_cast<Clock::duration>(std::chrono::microseconds(self->announceInterval.count() * 1000))) {
                self->sendAnnounce(communicator);
                self->lastAnnounceTime = now;
            }

            // Executa o algoritmo de eleição do grandmaster periodicamente
            if (now - self->lastBMCATime >= std::chrono::duration_cast<Clock::duration>(std::chrono::microseconds(self->bmcaInterval.count() * 1000))) {
                std::cout << "\nRodando BMCA" << std::endl;
                self->runBMCA();
                self->lastBMCATime = now;
            }

            // Mostra o relógio local corrigido periodicamente
            if (now - self->lastShareTime >= std::chrono::duration_cast<Clock::duration>(std::chrono::microseconds(self->shareTimeInterval.count() * 1000))) {
                self->lastShareTime = now;

                if (self->isGrandmaster) {
                    std::cout << "*(GC) ";
                    self->print_time_utc();
                } else {
                    std::cout << "(S) ";
                    self->print_time_utc();
                }
            }
        }

        self->dataPublisher->unsubscribe(communicator.getObserver());
        delete data;
        pthread_exit(nullptr);
    }

    // Processa mensagens PTP recebidas
    void processPTPMessage(Message* message, Communicator* communicator) {
        Message msg;
        switch (message->getType()) {
            case Ethernet::TYPE_PTP_SYNC:
                //std::cout << "Sync recebido" << std::endl;
                syncSendTime = message->getTimestamp();
                syncRecvTime = now();

                msg.setType(Ethernet::TYPE_PTP_DELAY_REQ);
                msg.setDstAddress(grandmasterAddress);
                msg.setPeriod(0);
                communicator->send(&msg);

                delayReqSendTime = msg.getTimestamp();
                break;

            case Ethernet::TYPE_PTP_ANNOUNCE:
                bmcaParticipants.insert(message->getSrcAddress());
                break;

            case Ethernet::TYPE_PTP_DELAY_REQ:
                if (isGrandmaster) {
                    //std::cout << "Delay REQ recebido" << std::endl;
                    msg.setType(Ethernet::TYPE_PTP_DELAY_RESP);
                    msg.setDstAddress(message->getSrcAddress());
                    msg.setPeriod(0);
                    communicator->send(&msg);
                    //std::cout << "Delay REQ respondido" << std::endl;
                }
                break;

            case Ethernet::TYPE_PTP_DELAY_RESP:
            {
                //std::cout << "Delay RESP recebido" << std::endl;
                delayRespRecvTime = message->getTimestamp();
                // Cálculo do offset baseado nas fórmulas do PTP
                // t1: syncSendTime (GM)       t2: syncRecvTime (Slave)
                // t3: delayReqSendTime (Slave) t4: delayRespRecvTime (GM)

                auto t1 = syncSendTime;
                auto t2 = syncRecvTime;
                auto t3 = delayReqSendTime;
                auto t4 = delayRespRecvTime;

                // Calcula offset e delay corretamente
                auto offset = std::chrono::duration_cast<std::chrono::microseconds>(((t2 - t1) - (t4 - t3)) / 2);
                //auto delay  = std::chrono::duration_cast<std::chrono::microseconds>((t2 - t1 + (t4 - t3)) / 2);

                clockOffset = offset;

                std::cout << "\n(S) Offset ajustado: " << clockOffset.count() << " µs (";
                print_address(address.vehicle_id);
                std::cout << ")" << std::endl;
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

    // Eleição do Grandmaster usando BMCA (Best Master Clock Algorithm)
    // criterio: Candidato com maior vehicle_id (mac_address ficticio da nic) 
    void runBMCA() {
        clockOffset = std::chrono::microseconds(0);

        if (bmcaParticipants.empty()) {
            std::cout << "Nenhum participante encontrado para BMCA" << std::endl;
            isGrandmaster = true;
            grandmasterAddress = address;
            std::cout << "Grandmaster eleito: ";
            print_address(grandmasterAddress.vehicle_id);
            std::cout << "\n" << std::endl;
            return;
        }

        // Seleciona o maior endereço como Grandmaster (critério simples)
        auto grandmaster = address;
        for (const auto& participant : bmcaParticipants) {
            if (participant > grandmaster) {
                grandmaster = participant;
            }
        }
        grandmasterAddress = grandmaster;

        isGrandmaster = (grandmaster == address);

        std::cout << "Grandmaster eleito: ";
        print_address(grandmasterAddress.vehicle_id);
        std::cout << "\n";

        bmcaParticipants.clear();
    }

    // Envia mensagem de sincronização
    void sendSync(Communicator& communicator) {
        Message msg;
        msg.setDstAddress({{0,0,0,0,0,0}, (pthread_t)0});
        msg.setType(Ethernet::TYPE_PTP_SYNC);
        msg.setPeriod(0);
        communicator.send(&msg);
        //std::cout << "Sync enviado" << std::endl;
    }

    // Envia mensagem de anúncio
    void sendAnnounce(Communicator& communicator) {
        Message msg;
        msg.setDstAddress({{0,0,0,0,0,0}, (pthread_t)0});
        msg.setType(Ethernet::TYPE_PTP_ANNOUNCE);
        msg.setPeriod(0);
        communicator.send(&msg);
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

    // Identidade do componente (MAC + thread ID)
    pthread_t thread;
    Ethernet::Address address;
    Ethernet::Address grandmasterAddress;

    std::vector<Ethernet::Type> types;  // Tipos de mensagens PTP observados
    bool isGrandmaster = false;
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

    // Intervalos de tempo para eventos periódicos
    std::chrono::milliseconds syncInterval{100};
    Clock::time_point lastSyncTime = Clock::now();

    std::chrono::milliseconds announceInterval{500};
    Clock::time_point lastAnnounceTime = Clock::now();

    std::chrono::milliseconds bmcaInterval{3000};
    Clock::time_point lastBMCATime = Clock::now();

    std::chrono::milliseconds shareTimeInterval{1000};
    Clock::time_point lastShareTime = Clock::now();

    // Conjunto para armazenar participantes do BMCA detectados por mensagens Announce
    std::set<Ethernet::Address> bmcaParticipants;
};
