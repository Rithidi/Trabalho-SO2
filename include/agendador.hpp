#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <set>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <utility>

#include "../include/ethernet.hpp"
#include "../include/communicator.hpp"
#include "../include/message.hpp"

using Address = Ethernet::Address;
using Thread_ID = Ethernet::Thread_ID;

class Agendador {
public:
    using Clock = std::chrono::steady_clock;

    // Estrutura que representa um interesse periódico para envio de mensagem
    struct Periodo {
        Thread_ID componente_id;             // ID do componente interessado
        Address destino;                     // Endereço de destino da mensagem
        std::chrono::milliseconds periodo;  // Intervalo entre envios
        Clock::time_point proximo_envio;    // Próximo momento para enviar

        // Operador < para ordenar Periodos no set
        bool operator<(const Periodo& other) const;

        // Operador == para comparações
        bool operator==(const Periodo& other) const;
    };

    // Hash para Periodo para uso em unordered_map
    struct HashPeriodo {
        std::size_t operator()(const Periodo& p) const noexcept;
    };

    // Hash para par (Thread_ID, Periodo) para estatísticas
    struct HashPair {
        std::size_t operator()(const std::pair<Thread_ID, Periodo>& p) const noexcept;
    };

    // Estrutura para destinos prontos de um componente
    struct PeriodosAtingidos {
        std::mutex mutex;                     // Protege acesso a destinos_prontos
        std::vector<Address> destinos_prontos; // Destinos cujo período já foi atingido
        std::atomic<bool> pronto{false};     // Indica se há destinos prontos
    };

    // Estrutura para armazenar estatísticas de envio
    struct EstatisticaEnvio {
        std::vector<Clock::time_point> tempos;       // Registros dos tempos de envio
        std::chrono::milliseconds periodo_desejado;  // Período esperado para envio
    };

    // Construtor que inicia a thread do loop
    Agendador();

    // Destrutor que finaliza a thread e imprime estatísticas
    ~Agendador();

    // Registra interesse periódico para um componente e destino
    void registrar_interesse(Thread_ID componente_id, Address destino, Ethernet::Period periodo);

    // Remove interesse periódico para componente e destino
    void remover_interesse(Thread_ID componente_id, Address destino);

    // Verifica se componente tem períodos atingidos
    bool possui_periodos_atingidos(Thread_ID componente_id);

    // Obtém destinos prontos para um componente e limpa lista interna
    std::vector<Address> obter_destinos_prontos(Thread_ID componente_id);

    // Imprime estatísticas do agendador
    void imprimir_estatisticas();

private:
    // Loop principal da thread para controlar envios periódicos
    void loop();

    std::set<Periodo> periodos_;  // Conjunto ordenado dos períodos ativos
    std::unordered_map<Thread_ID, PeriodosAtingidos> componentes_registrados_; // Destinos prontos por componente
    std::unordered_map<std::pair<Thread_ID, Periodo>, EstatisticaEnvio, HashPair> estatisticas_; // Estatísticas de envio

    std::mutex mutex_;                // Protege estruturas compartilhadas
    std::condition_variable cv_;     // Para acordar thread em espera
    std::thread thread_;             // Thread do agendador
    bool stop_;                      // Indica quando parar o loop
};
