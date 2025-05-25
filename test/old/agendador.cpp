#include "../include/agendador.hpp"

#include <iostream>
#include <cmath>

// Implementação operador < para ordenar Periodo no set
bool Agendador::Periodo::operator<(const Periodo& other) const {
    if (proximo_envio == other.proximo_envio) {
        if (componente_id == other.componente_id) {
            return destino.component_id < other.destino.component_id;
        }
        return componente_id < other.componente_id;
    }
    return proximo_envio < other.proximo_envio;
}

// Implementação operador == para comparação de Periodo
bool Agendador::Periodo::operator==(const Periodo& other) const {
    return componente_id == other.componente_id &&
           destino.component_id == other.destino.component_id &&
           periodo == other.periodo;
}

// Hash para Periodo para uso em unordered_map
std::size_t Agendador::HashPeriodo::operator()(const Periodo& p) const noexcept {
    std::size_t h1 = std::hash<Thread_ID>{}(p.componente_id);
    std::size_t h2 = std::hash<int>{}(p.destino.component_id);
    std::size_t h3 = std::hash<long long>{}(p.periodo.count());
    return h1 ^ (h2 << 1) ^ (h3 << 2);
}

// Hash para par (Thread_ID, Periodo) para estatísticas
std::size_t Agendador::HashPair::operator()(const std::pair<Thread_ID, Periodo>& p) const noexcept {
    std::size_t h1 = std::hash<Thread_ID>{}(p.first);
    HashPeriodo hPeriodo;
    std::size_t h2 = hPeriodo(p.second);
    return h1 ^ (h2 << 1);
}

// Construtor - inicia thread do loop
Agendador::Agendador() : stop_(false) {
    thread_ = std::thread(&Agendador::loop, this);
}

// Destrutor - para thread e imprime estatísticas
Agendador::~Agendador() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;    // sinaliza para parar o loop
    }
    cv_.notify_all();    // acorda thread se estiver esperando
    if (thread_.joinable()) {
        thread_.join();  // espera thread terminar
    }
    imprimir_estatisticas();
}

// Imprime estatísticas dos períodos de envio
void Agendador::imprimir_estatisticas() {
    if (estatisticas_.empty()) {
        return;
    }

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
        double diferenca = std::abs(periodo.periodo.count() - media);

        soma_diferencas += diferenca;
        total_amostras++;

        cont_registros++;
        std::cout << "ID Registro: " << cont_registros
                  << " | Periodo desejado: " << periodo.periodo.count() << " ms"
                  << " | Média intervalos: " << media << " ms"
                  << " | Diferença média: " << diferenca << " ms"
                  << " | Nº de notificações: " << tempos.size() << "\n";
    }

    if (total_amostras > 0) {
        std::cout << "\nMédia global das diferenças entre períodos desejados e intervalos de notificação: "
                  << (soma_diferencas / total_amostras) << " ms\n";
    }
}

// Registra interesse periódico para um componente e destino
void Agendador::registrar_interesse(Thread_ID componente_id, Address destino, Ethernet::Period periodo) {
    std::chrono::milliseconds periodo_ms(periodo);

    std::lock_guard<std::mutex> lock(mutex_);
    Periodo p = {componente_id, destino, periodo_ms, Clock::now() + periodo_ms};
    periodos_.insert(p);

    estatisticas_[{componente_id, p}].periodo_desejado = periodo_ms;

    cv_.notify_all();
}

// Remove interesse periódico de um componente para um destino
void Agendador::remover_interesse(Thread_ID componente_id, Address destino) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = periodos_.begin(); it != periodos_.end(); ++it) {
        if (it->componente_id == componente_id && it->destino.component_id == destino.component_id) {
            periodos_.erase(it);
            break;
        }
    }
}

// Verifica se componente tem períodos atingidos
bool Agendador::possui_periodos_atingidos(Thread_ID componente_id) {
    return componentes_registrados_[componente_id].pronto.load();
}

// Retorna destinos prontos para componente e limpa lista interna
std::vector<Address> Agendador::obter_destinos_prontos(Thread_ID componente_id) {
    if (!possui_periodos_atingidos(componente_id)) {
        return {};
    }

    auto& atingidos = componentes_registrados_[componente_id];

    std::lock_guard<std::mutex> lock(atingidos.mutex);
    std::vector<Address> prontos;
    prontos.swap(atingidos.destinos_prontos);
    atingidos.pronto.store(false);

    return prontos;
}

// Loop principal da thread para controlar os envios periódicos
void Agendador::loop() {
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