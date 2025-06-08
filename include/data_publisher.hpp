#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <vector>
#include <unordered_map>

#include "../include/ethernet.hpp"
#include "../include/communicator.hpp"
#include "../include/message.hpp"
#include "../include/observer.hpp"

// Classe responsável por gerenciar a entrega de mensagens periódicas para observadores interessados.
class DataPublisher {
    // Estrutura que armazena os recursos de controle de uma thread periódica.
    struct ThreadControl {
        std::thread thread;                    // Thread que envia mensagens periodicamente.
        Message message;                       // Mensagem que sera enviada periodicamente.
        std::condition_variable* cv;           // Variável de condição usada para interromper a espera.
        std::mutex* mtx;                       // Mutex usada junto da variável de condição.
        bool* stop_flag;                       // Flag que sinaliza a thread para parar.
    };

public:
    // Permite que um componente se inscreva para receber mensagens de certos tipos.
    void subscribe(Concurrent_Observer* obsCommunicator, std::vector<Ethernet::Type>* types);

    // Cancela a inscrição de um componente e encerra suas threads periódicas.
    void unsubscribe(Concurrent_Observer* obsCommunicator);

    // Recebe uma nova mensagem e distribui para os componentes interessados.
    void notify(Message message);

    // Encerra todas as threads periodicas associadas a um determinado grupo.
    void delete_group_threads(Ethernet::Group_ID group_id);

private:
    // Cria uma thread periódica que envia uma mensagem a um observador em intervalos fixos.
    void create_periodic_thread(Concurrent_Observer* obsCommunicator, Message message);

    // Encerra e remove todas as threads periódicas associadas a um observador.
    void delete_periodic_thread(Concurrent_Observer* obsCommunicator);

    // Função que roda em cada thread periódica, enviando mensagens até receber sinal de parada.
    void publish_loop(std::condition_variable* cv, std::mutex* mtx, bool* stop_flag,
                      Concurrent_Observer* obsCommunicator, Message message,
                      std::chrono::milliseconds period_ms);

private:
    // Mapeia cada observador aos tipos de mensagens que ele deseja receber.
    std::unordered_map<Concurrent_Observer*, std::vector<Ethernet::Type>*> subscribers;
    std::mutex subscribers_mutex; // Protege o acesso ao mapa de inscritos.

    // Mapeia cada observador às threads que estão enviando mensagens periodicamente para ele.
    std::unordered_map<Concurrent_Observer*, std::vector<ThreadControl>> threads;
    std::mutex threads_mutex; // Protege o acesso ao mapa de threads.
};
