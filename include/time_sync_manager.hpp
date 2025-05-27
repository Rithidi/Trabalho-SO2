#include <pthread.h>
#include <set>
#include <vector>
#include <chrono>
#include <tuple>

#include "../include/communicator.hpp"
#include "../include/ethernet.hpp"
#include "../include/protocol.hpp"
#include "../include/data_publisher.hpp"
#include "../include/message.hpp"

class TimeSyncManager {
public:
    // Estrutura usada para passar dados para a thread POSIX
    struct ThreadData {
        TimeSyncManager* instance;  // Ponteiro para a instância da classe
    };

    // Construtor da classe
    TimeSyncManager(DataPublisher* dataPublisher, Protocol* protocol, Ethernet::Mac_Address vehicleID)
        : dataPublisher(dataPublisher), protocol(protocol), running(true) {

        // Inicializa os tipos de mensagem PTP que o manager irá monitorar
        types.push_back(Ethernet::TYPE_PTP_ANNOUNCE);
        types.push_back(Ethernet::TYPE_PTP_SYNC);
        types.push_back(Ethernet::TYPE_PTP_DELAY_REQ);
        types.push_back(Ethernet::TYPE_PTP_DELAY_RESP);

        // Cria estrutura com ponteiro para esta instância para passar à thread
        ThreadData* data = new ThreadData{this};

        // Cria a thread que executa a rotina de sincronização de tempo
        pthread_create(&thread, nullptr, &TimeSyncManager::time_sync_routine, data);

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
    std::chrono::steady_clock::time_point now() {
        return std::chrono::steady_clock::now() + clockOffset;
    }

private:
    // Função que será executada pela thread (deve ser estática)
    static void* time_sync_routine(void* arg) {
        // Recupera a estrutura com o ponteiro para a instância
        ThreadData* data = static_cast<ThreadData*>(arg);
        TimeSyncManager* self = data->instance;

        // Cria o comunicador para troca de mensagens na thread
        Communicator communicator(self->protocol, self->address.vehicle_id, pthread_self());

        // Inscreve o comunicador para receber mensagens dos tipos especificados
        self->dataPublisher->subscribe(communicator.getObserver(), &self->types);

        // Loop principal da thread que ficará escutando e enviando mensagens
        while (self->running) {
            if (communicator.hasMessage()) {
                Message msg;
                communicator.receive(&msg);                  // Recebe mensagem
                self->processPTPMessage(&msg, communicator); // Processa mensagem recebida
            }

            auto now = std::chrono::steady_clock::now();

            // Se é grandmaster e passou intervalo para enviar sync, envia
            if (self->isGrandmaster && ((now - self->lastSyncTime) >= self->syncInterval)) {
                self->sendSync(communicator);
                self->lastSyncTime = now;
            }

            // Se passou intervalo para enviar announce, envia
            if ((now - self->lastAnnounceTime) >= self->announceInterval) {
                self->sendAnnounce(communicator);
                self->lastAnnounceTime = now;
            }

            // Se passou intervalo para rodar BMCA (eleição do grandmaster), roda
            if ((now - self->lastBMCATime) >= self->bmcaInterval) {
                self->runBMCA();
                self->lastBMCATime = now;
            }
        }

        // Remove inscrição para mensagens antes de encerrar
        self->dataPublisher->unsubscribe(communicator.getObserver());

        // Libera memória alocada para passagem de dados da thread
        delete data;

        pthread_exit(nullptr);
    }

    // Processa as mensagens PTP recebidas
    void processPTPMessage(Message* message, Communicator& communicator) {
        Message msg;
        switch (message->getType()) {
            case Ethernet::TYPE_PTP_SYNC:
                // Recebeu mensagem Sync, salva tempos para cálculo de offset
                syncSendTime = message->getTimestamp();
                syncRecvTime = std::chrono::steady_clock::now();

                // Envia mensagem Delay Request para o grandmaster
                msg.setType(Ethernet::TYPE_PTP_DELAY_REQ);
                msg.setTimestamp(std::chrono::steady_clock::now());
                msg.setDstAddress(grandmasterAddress);
                msg.setPeriod(0);

                delayReqSendTime = msg.getTimestamp();

                communicator.send(&msg);
                break;

            case Ethernet::TYPE_PTP_ANNOUNCE:
                // Recebeu announce, adiciona participante para BMCA
                bmcaParticipants.insert(message->getSrcAddress());
                break;

            case Ethernet::TYPE_PTP_DELAY_REQ:
                // Recebeu delay request, envia delay response
                msg.setType(Ethernet::TYPE_PTP_DELAY_RESP);
                msg.setDstAddress(message->getSrcAddress());
                msg.setTimestamp(std::chrono::steady_clock::now());
                msg.setPeriod(0);
                communicator.send(&msg);
                break;

            case Ethernet::TYPE_PTP_DELAY_RESP:
                // Recebeu delay response, calcula offset do relógio local
                delayRespRecvTime = message->getTimestamp();
                // Calcula o offset do relógio local 
                // roundTrip = (delayRespRecvTime - delayReqSendTime) + (syncRecvTime - syncSendTime)
                // delay = roundTrip / 2
                // clockOffset = (syncRecvTime - syncSendTime) - delay 
                // delay é uma aproximacao da media do tempo de ida e volta das mensagens, utilizando delay response e o sync.
                clockOffset = (syncRecvTime - syncSendTime) - (delayRespRecvTime - delayReqSendTime) + (syncRecvTime - syncSendTime) / 2;
                break;

            default:
                // Ignora outros tipos de mensagem
                break;
        }
    }

    // Função que executa a eleição do grandmaster via BMCA
    void runBMCA() {
        if (bmcaParticipants.empty()) {
            isGrandmaster = true;
            return;
        }

        // Seleciona o maior endereço como grandmaster
        auto grandmaster = *bmcaParticipants.begin();
        for (const auto& participant : bmcaParticipants) {
            if (participant > grandmaster) {
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

        // Limpa participantes para próxima rodada
        bmcaParticipants.clear();
    }

    // Envia mensagem Sync multicast
    void sendSync(Communicator& communicator) {
        Message msg;
        msg.setDstAddress({{0, 0, 0, 0, 0, 0}, (pthread_t)0});
        msg.setType(Ethernet::TYPE_PTP_SYNC);
        msg.setPeriod(0);
        msg.setTimestamp(std::chrono::steady_clock::now());
        communicator.send(&msg);
    }

    // Envia mensagem Announce multicast
    void sendAnnounce(Communicator& communicator) {
        Message msg;
        msg.setDstAddress({{0, 0, 0, 0, 0, 0}, (pthread_t)0});
        msg.setType(Ethernet::TYPE_PTP_ANNOUNCE);
        msg.setPeriod(0);
        communicator.send(&msg);
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
    std::chrono::steady_clock::time_point syncSendTime;
    std::chrono::steady_clock::time_point syncRecvTime;
    std::chrono::steady_clock::time_point delayReqSendTime;
    std::chrono::steady_clock::time_point delayRespRecvTime;

    // Offset de relógio calculado
    std::chrono::nanoseconds clockOffset = std::chrono::nanoseconds(0);

    // Intervalos de temporização para envio das mensagens e BMCA
    std::chrono::milliseconds syncInterval = std::chrono::milliseconds(250);
    std::chrono::steady_clock::time_point lastSyncTime = std::chrono::steady_clock::now();

    std::chrono::milliseconds announceInterval = std::chrono::milliseconds(1000);
    std::chrono::steady_clock::time_point lastAnnounceTime = std::chrono::steady_clock::now();

    std::chrono::milliseconds bmcaInterval = std::chrono::milliseconds(500);
    std::chrono::steady_clock::time_point lastBMCATime = std::chrono::steady_clock::now();

    // Conjunto de participantes para BMCA
    std::set<Ethernet::Address> bmcaParticipants;
};
