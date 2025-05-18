#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <set>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <atomic>
#include <utility> // para std::pair

#include "../include/ethernet.hpp"
#include "../include/communicator.hpp"
#include "../include/message.hpp"

using namespace std::chrono_literals;
using Address = Ethernet::Address;
using Thread_ID = Ethernet::Thread_ID;

class Agendador {
public:
    using Clock = std::chrono::steady_clock;

    // Estrutura que representa um interesse com período de envio repetido
    struct Periodo {
        Thread_ID componente_id;             // ID do componente que se interessou
        Address destino;                     // Endereço destino da mensagem
        std::chrono::milliseconds periodo;  // Intervalo de tempo entre envios
        Clock::time_point proximo_envio;    // Próximo momento para enviar mensagem

        // Operador para ordenar Periodos no set, baseado no próximo envio, componente e destino
        bool operator<(const Periodo& other) const {
            if (proximo_envio == other.proximo_envio) {
                if (componente_id == other.componente_id) {
                    // evita duplicatas iguais, compara destinos
                    return destino.component_id < other.destino.component_id;
                }
                return componente_id < other.componente_id;
            }
            return proximo_envio < other.proximo_envio;
        }

        // Operador de igualdade usado em algumas operações
        bool operator==(const Periodo& other) const {
            return componente_id == other.componente_id &&
                   destino.component_id == other.destino.component_id &&
                   periodo == other.periodo;
        }
    };

    // Hash para Periodo para usar em unordered_map
    struct HashPeriodo {
        std::size_t operator()(const Periodo& p) const noexcept {
            std::size_t h1 = std::hash<Thread_ID>{}(p.componente_id);
            std::size_t h2 = std::hash<int>{}(p.destino.component_id); // assume componente_id é int
            std::size_t h3 = std::hash<long long>{}(p.periodo.count());
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };

    // Hash para a chave pair<Thread_ID, Periodo> usado nas estatísticas
    struct HashPair {
        std::size_t operator()(const std::pair<Thread_ID, Periodo>& p) const noexcept {
            std::size_t h1 = std::hash<Thread_ID>{}(p.first);
            HashPeriodo hPeriodo;
            std::size_t h2 = hPeriodo(p.second);
            return h1 ^ (h2 << 1);
        }
    };

    // Estrutura para guardar destinos prontos para um componente, com controle de concorrência
    struct PeriodosAtingidos {
        std::mutex mutex;                     // Protege acesso ao vetor destinos_prontos
        std::vector<Address> destinos_prontos; // Destinos cujo período já atingiu
        std::atomic<bool> pronto{false};     // Flag que indica se há destinos prontos
    };

    // Estrutura para armazenar tempos de envio e período desejado para estatística
    struct EstatisticaEnvio {
        std::vector<Clock::time_point> tempos;       // Registros dos tempos de envio
        std::chrono::milliseconds periodo_desejado;  // Período que se espera respeitar
    };

    // Construtor inicia a thread do loop do agendador
    Agendador() : stop_(false) {
        thread_ = std::thread(&Agendador::loop, this);
    }

    // Destrutor para encerrar a thread e mostrar estatísticas
    ~Agendador() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_ = true;    // Sinaliza para parar o loop
        }
        cv_.notify_all();    // Acorda thread se estiver esperando
        if (thread_.joinable()) {
            thread_.join();  // Aguarda thread terminar
        }
        imprimir_estatisticas();
    }

    void imprimir_estatisticas() {
        // Verifica se tem estatistica para imprimir.
        if (!estatisticas_.empty()) {
            // Imprime estatísticas de periodicidade dos envios
            std::cout << "\n=== ESTATÍSTICAS DO AGENDADOR ===\n";
            double soma_diferencas = 0.0;
            int total_amostras = 0;

            int cont_registros = 0;

            for (const auto& [key, estat] : estatisticas_) {
                const auto& [componente_id, periodo] = key;
                const auto& tempos = estat.tempos;
                if (tempos.size() < 2) continue;

                double soma = 0.0;
                for (size_t i = 1; i < tempos.size(); ++i) {
                    auto intervalo = std::chrono::duration_cast<std::chrono::milliseconds>(tempos[i] - tempos[i - 1]).count();
                    soma += intervalo;
                }

                double media = soma / (tempos.size() - 1);
                double diferenca = std::abs(periodo.periodo.count() - media); // diferença média entre período desejado e real
                soma_diferencas += diferenca;
                total_amostras++;
                
                cont_registros++;
                std::cout << "ID Registro: " << cont_registros
                //std::cout << "Componente responsável: " << componente_id
                //        << " | Destino componente_id: " << periodo.destino.component_id
                        << " | Periodo desejado: " << periodo.periodo.count() << " ms"
                        << " | Média intervalos: " << media << " ms"
                        << " | Diferença média: " << diferenca << " ms"
                        << " | Nº de notificações: " << tempos.size() << "\n";
            }

            if (total_amostras > 0) {
                std::cout << "\nMédia global das diferenças entre períodos desejados e intervalos de notificacao: "
                        << (soma_diferencas / total_amostras) << " ms\n";
            }
        }
    }

    // Registra interesse periódico de um componente para um destino
    void registrar_interesse(Thread_ID componente_id, Address destino, Ethernet::Period periodo) {
        std::chrono::milliseconds periodo_ms(periodo);

        std::lock_guard<std::mutex> lock(mutex_);
        Periodo p = {componente_id, destino, periodo_ms, Clock::now() + periodo_ms};
        periodos_.insert(p); // adiciona o período ao conjunto

        // Registra período para estatísticas
        estatisticas_[{componente_id, p}].periodo_desejado = periodo_ms;

        cv_.notify_all();  // Acorda a thread do loop para processar novos períodos
    }

    // Remove interesse periódico de um componente para um destino
    void remover_interesse(Thread_ID componente_id, Address destino) {
        std::lock_guard<std::mutex> lock(mutex_);

        for (auto it = periodos_.begin(); it != periodos_.end(); ++it) {
            if (it->componente_id == componente_id && it->destino.component_id == destino.component_id) {
                periodos_.erase(it);
                break;
            }
        }
    }

    // Verifica se há períodos atingidos para determinado componente
    bool possui_periodos_atingidos(Thread_ID componente_id) {
        return componentes_registrados_[componente_id].pronto.load();
    }

    // Retorna destinos cujos períodos foram atingidos para um componente e limpa a lista
    std::vector<Address> obter_destinos_prontos(Thread_ID componente_id) {
        if (!possui_periodos_atingidos(componente_id)) {
            return {}; // Nenhum destino pronto
        }

        std::vector<Address> prontos;
        auto& atingidos = componentes_registrados_[componente_id];

        std::lock_guard<std::mutex> lock(atingidos.mutex);
        prontos.swap(atingidos.destinos_prontos); // Troca conteúdo para devolver e limpar vetor original
        atingidos.pronto.store(false);            // Marca que não há mais destinos prontos
        return prontos;
    }

private:
    // Função executada pela thread para controlar os envios periódicos
    void loop() {
        std::unique_lock<std::mutex> lock(mutex_);

        while (!stop_) {
            if (periodos_.empty()) {
                cv_.wait(lock);  // Espera até que haja algum período registrado
                continue;
            }

            // Pega o período que deve ser executado primeiro
            auto it = periodos_.begin();
            Periodo p = *it;
            auto agora = Clock::now();

            if (agora >= p.proximo_envio) {
                // Remove período da lista temporariamente
                periodos_.erase(it);

                // Marca o destino como pronto para o componente
                auto& atingidos = componentes_registrados_[p.componente_id];
                {
                    std::lock_guard<std::mutex> lock_atingidos(atingidos.mutex);
                    atingidos.destinos_prontos.push_back(p.destino);
                    atingidos.pronto.store(true); // sinaliza que tem destino pronto
                }

                // Armazena estatística de tempo de envio
                estatisticas_[{p.componente_id, p}].tempos.push_back(agora);

                // Atualiza o próximo envio e reinserir período para reuso
                p.proximo_envio += p.periodo;
                periodos_.insert(p);

                continue;
            }

            // Espera até o próximo envio do período
            cv_.wait_until(lock, p.proximo_envio);
        }
    }

    std::set<Periodo> periodos_;  // Conjunto ordenado dos períodos ativos
    std::unordered_map<Thread_ID, PeriodosAtingidos> componentes_registrados_; // Destinos prontos por componente

    // Estatísticas de envios, chave é par (Thread_ID, Periodo)
    std::unordered_map<std::pair<Thread_ID, Periodo>, EstatisticaEnvio, HashPair> estatisticas_;

    std::mutex mutex_;                // Protege acesso a estruturas compartilhadas
    std::condition_variable cv_;     // Para acordar thread em espera
    std::thread thread_;             // Thread que executa o loop do agendador
    bool stop_;                      // Sinaliza parada do loop
};
