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

class TimeSyncManager {
public:
    using Clock = std::chrono::system_clock;

    // Estrutura usada para passar dados para a thread POSIX
    struct ThreadData {
        TimeSyncManager* instance;  // Ponteiro para a instância da classe
    };

    // Construtor da classe
    TimeSyncManager(DataPublisher* dataPublisher, Protocol* protocol, Ethernet::Mac_Address vehicleID)
        : dataPublisher(dataPublisher), protocol(protocol), running(true) {

        print_address(vehicleID);
        std::cout << " | Offset inicial do relógio: " << defaultClockOffset.count() << " ns" << std::endl;

        // Inicializa os tipos de mensagem PTP que o manager irá monitorar
        types.push_back(Ethernet::TYPE_PTP_ANNOUNCE);
        types.push_back(Ethernet::TYPE_PTP_SYNC);
        types.push_back(Ethernet::TYPE_PTP_DELAY_REQ);
        types.push_back(Ethernet::TYPE_PTP_DELAY_RESP);

        // Cria estrutura com ponteiro para esta instância para passar à thread
        ThreadData* data = new ThreadData{this};

        // Cria a thread que executa a rotina de sincronização de tempo
        int err = pthread_create(&thread, nullptr, &TimeSyncManager::time_sync_routine, data);
        if (err != 0) {
            std::cerr << "Erro ao criar thread: " << strerror(err) << std::endl;
        }

        // Inicializa o endereço do componente com o ID do veículo e o ID da thread
        address.vehicle_id = vehicleID;
        address.component_id = thread;
    }

    // Destrutor da classe para garantir encerramento da thread
    ~TimeSyncManager() {
        running = false;                // Sinaliza para a thread parar
        pthread_join(thread, nullptr);  // Espera a thread finalizar
    }

    // Retorna o horário atual corrigido pelo offset calculado
    Clock::time_point now() {
        // tempo sincronizado = tempo atual do sistema + offset total
        // offset total = defaultClockOffset + gmClockOffset
        return Clock::now() + std::chrono::duration_cast<Clock::duration>(defaultClockOffset) +
                              std::chrono::duration_cast<Clock::duration>(clockOffset);
    }

private:
    // Função que será executada pela thread
    static void* time_sync_routine(void* arg) {
        // Recupera a estrutura com o ponteiro para a instância
        ThreadData* data = static_cast<ThreadData*>(arg);
        TimeSyncManager* self = data->instance;

        // Cria o comunicador para troca de mensagens na thread
        Communicator communicator(self->protocol, self->address.vehicle_id, pthread_self());

        // Inscreve o comunicador para receber mensagens dos tipos especificados
        self->dataPublisher->subscribe(communicator.getObserver(), &self->types);

        // Envia mensagem sync para iniciar a sincronização
        self->sendAnnounce(communicator);
        self->lastAnnounceTime = Clock::now();
        std::cout << "Announce enviado" << std::endl;

        // Loop principal da thread que ficará escutando e enviando mensagens
        while (self->running) {
            if (communicator.hasMessage()) {
                Message msg;
                communicator.receive(&msg);                  // Recebe mensagem
                self->processPTPMessage(&msg, &communicator); // Processa mensagem recebida
            }

            auto now = Clock::now();

            // Se é grandmaster e passou intervalo para enviar sync, envia
            if (self->isGrandmaster && ((now - self->lastSyncTime) >= self->syncInterval)) {
                self->sendSync(communicator);
                self->lastSyncTime = now;
            }

            // Se passou intervalo para enviar announce, envia
            if ((now - self->lastAnnounceTime) >= self->announceInterval) {
                self->sendAnnounce(communicator);
                self->lastAnnounceTime = now;
                //std::cout << "Enviando mensagem Announce" << std::endl;
            }

            // Se passou intervalo para rodar BMCA (eleição do grandmaster), roda
            if ((now - self->lastBMCATime) >= self->bmcaInterval) {
                std::cout << "Rodando BMCA" << std::endl;
                // Roda BMCA para eleger o grandmaster
                self->runBMCA();
                self->lastBMCATime = now;
            }

            if ((now - self->lastShareTime) >= self->shareTimeInterval) {
                self->lastShareTime = now;

                auto time = self->now();

                if (self->isGrandmaster) {
                    std::cout << "GM" << std::endl;
                    self->print_time_utc();
                } else {
                    std::cout << "M" << std::endl;
                    self->print_time_utc();
                }
            }

        }

        // Remove inscrição para mensagens antes de encerrar
        self->dataPublisher->unsubscribe(communicator.getObserver());

        // Libera memória alocada para passagem de dados da thread
        delete data;

        pthread_exit(nullptr);
    }

    // Processa as mensagens PTP recebidas
    void processPTPMessage(Message* message, Communicator* communicator) {
        Message msg;
        switch (message->getType()) {
            case Ethernet::TYPE_PTP_SYNC:
                std::cout << "Sync recebido" << std::endl;
                // Recebeu mensagem Sync, salva tempos para cálculo de offset
                syncSendTime = message->getTimestamp();
                syncRecvTime = now();

                // Envia mensagem Delay Request para o grandmaster
                msg.setType(Ethernet::TYPE_PTP_DELAY_REQ);
                msg.setDstAddress(grandmasterAddress);
                msg.setPeriod(0);
                communicator->send(&msg);

                delayReqSendTime = msg.getTimestamp();
                break;

            case Ethernet::TYPE_PTP_ANNOUNCE:
                std::cout << "Announce recebido" << std::endl;
                //print_address(message->getSrcAddress().vehicle_id);
                // Recebeu announce, adiciona participante para BMCA
                bmcaParticipants.insert(message->getSrcAddress());
                break;

            case Ethernet::TYPE_PTP_DELAY_REQ:
                std::cout << "Delay REQ recebido" << std::endl;
                // Recebeu delay request, envia delay response
                msg.setType(Ethernet::TYPE_PTP_DELAY_RESP);
                msg.setDstAddress(message->getSrcAddress());
                msg.setPeriod(0);
                communicator->send(&msg);
                std::cout << "Delay REQ respondido" << std::endl;
                break;

            case Ethernet::TYPE_PTP_DELAY_RESP:
            {
                std::cout << "Delay RESP recebido" << std::endl;
                // Recebeu delay response, calcula offset do relógio local
                delayRespRecvTime = message->getTimestamp();

                auto delay = (delayRespRecvTime - delayReqSendTime + syncRecvTime - syncSendTime) / 2;
                clockOffset = ((syncRecvTime - syncSendTime) - (delayRespRecvTime - delayReqSendTime)) / 2;

                std::cout << "Novo Offset calculado: " << clockOffset.count() << " ns" << std::endl;    
                std::cout << "Delay: " << delay.count() << " ns" << std::endl;
                std::cout << "syncRecvTime: " << syncRecvTime.time_since_epoch().count() << " ns" << std::endl;
                std::cout << "syncSendTime: " << syncSendTime.time_since_epoch().count() << " ns" << std::endl;
                std::cout << "delayRespRecvTime: " << delayRespRecvTime.time_since_epoch().count() << " ns" << std::endl;
                std::cout << "delayReqSendTime: " << delayReqSendTime.time_since_epoch().count() << " ns" << std::endl;

                break;
            }

            default:
                // Ignora outros tipos de mensagem
                break;
        }
    }

    // Função que executa a eleição do grandmaster via BMCA
    void runBMCA() {
        // Restaura offset do relógio para zero antes de rodar BMCA
        clockOffset = std::chrono::nanoseconds(0);

        if (bmcaParticipants.empty()) {
            std::cout << "Nenhum participante encontrado para BMCA" << std::endl;
            isGrandmaster = true;
            grandmasterAddress = address;
            std::cout << "Grandmaster eleito: ";
            print_address(grandmasterAddress.vehicle_id);
            std::cout << "\n";
            return;
        }

        // Seleciona o maior endereço como grandmaster
        auto grandmaster = address; // Começa com o próprio endereço
        for (const auto& participant : bmcaParticipants) {
            if (participant > grandmaster) {
                //print_address(participant.vehicle_id);
                grandmaster = participant;
            }
        }
        grandmasterAddress = grandmaster;

        // Define se esta instância é o grandmaster
        if (grandmaster == address) {
            isGrandmaster = true;
        } else {
            isGrandmaster = false;
        }

        std::cout << "Grandmaster eleito: ";
        print_address(grandmasterAddress.vehicle_id);
        std::cout << "\n";

        // Limpa participantes para próxima rodada
        bmcaParticipants.clear();
    }

    // Envia mensagem Sync multicast
    void sendSync(Communicator& communicator) {
        Message msg;
        msg.setDstAddress({{0, 0, 0, 0, 0, 0}, (pthread_t)0});
        msg.setType(Ethernet::TYPE_PTP_SYNC);
        msg.setPeriod(0);
        communicator.send(&msg);
        std::cout << "Sync enviado" << std::endl;
    }

    // Envia mensagem Announce multicast
    void sendAnnounce(Communicator& communicator) {
        Message msg;
        msg.setDstAddress({{0, 0, 0, 0, 0, 0}, (pthread_t)0});
        msg.setType(Ethernet::TYPE_PTP_ANNOUNCE);
        msg.setPeriod(0);
        communicator.send(&msg);
        std::cout << "Announce enviado" << std::endl; 
    }

    void print_address(const Ethernet::Mac_Address& vehicle_id) {
        std::cout << "Vehicle ID: ";
        for (size_t i = 0; i < vehicle_id.size(); ++i) {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                    << static_cast<int>(vehicle_id[i]);
            if (i != vehicle_id.size() - 1) std::cout << ":";
        }
        //std::cout << " | Thread ID: " << addr.component_id << std::dec << std::endl;
    }

    void print_time_utc() {
        auto time_now = now();

        std::time_t time_t_now = std::chrono::system_clock::to_time_t(time_now);
        std::tm* gmt = std::gmtime(&time_t_now);  // UTC

        auto duration = time_now.time_since_epoch();
        auto micros = std::chrono::duration_cast<std::chrono::microseconds>(duration).count() % 1000000;

        int frac4 = static_cast<int>(micros / 100);

        std::cout << "Relogio atual: "
                << std::put_time(gmt, "%Y-%m-%d %H:%M:%S")
                << "." << std::setfill('0') << std::setw(4) 
                << std::dec << frac4 // força decimal aqui
                << " UTC" << std::endl;
    }

private:
    // Ponteiros para instâncias externas necessárias
    DataPublisher* dataPublisher;
    Protocol* protocol;

    // Handle da thread criada
    pthread_t thread;

    // Endereço desta instância
    Ethernet::Address address;

    // Endereço do grandmaster atual
    Ethernet::Address grandmasterAddress;

    // Tipos de mensagem que monitoramos
    std::vector<Ethernet::Type> types;

    // Flag indicando se esta instância é grandmaster
    bool isGrandmaster = false;

    // Flag para controlar execução da thread
    bool running = true;

    // Tempos para cálculo do offset do relógio
    Clock::time_point syncSendTime;
    Clock::time_point syncRecvTime;
    Clock::time_point delayReqSendTime;
    Clock::time_point delayRespRecvTime;

    // Offset de relógio calculado
    // gera offset aleatorio entre -1000 e 1000 ns
    std::random_device rd;
    std::mt19937 gen{rd()};
    std::uniform_int_distribution<int64_t> dist{10000, 100000};
    std::chrono::nanoseconds defaultClockOffset = std::chrono::nanoseconds(dist(gen));
    //std::chrono::nanoseconds defaultClockOffset = std::chrono::nanoseconds(0);
    std::chrono::nanoseconds clockOffset = std::chrono::nanoseconds(0);

    // Intervalos de temporização para envio das mensagens e BMCA
    std::chrono::milliseconds syncInterval = std::chrono::milliseconds(5000);
    Clock::time_point lastSyncTime = Clock::now();

    std::chrono::milliseconds announceInterval = std::chrono::milliseconds(1000);
    Clock::time_point lastAnnounceTime = Clock::now();

    std::chrono::milliseconds bmcaInterval = std::chrono::milliseconds(5000);
    Clock::time_point lastBMCATime = Clock::now();

    // Intervalo de comunicacao do relogio atual (APENAS PARA TESTE).
    std::chrono::milliseconds shareTimeInterval = std::chrono::milliseconds(6000);
    Clock::time_point lastShareTime = Clock::now();
    
    // Conjunto de participantes para BMCA
    std::set<Ethernet::Address> bmcaParticipants;
};
