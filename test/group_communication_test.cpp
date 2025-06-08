#include "../include/rsu.hpp"
#include "../include/vehicle.hpp"

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <iostream>

// Parametros de teste.
int INTERVALO_CRIACAO = 500;    // Intervalo de tempo (ms) entre a criação dos veículos.
int INTERVALO_POSICAO = 2000;   // Intervalo de tempo (ms) em que o veículo dinamico avança sua posição.
int NUM_VOLTAS = 1;             // Número de voltas que o veículo dinâmico realiza durante o teste.
int PERIODO_DETECCAO = 500;    // Periodo de envio das mensagens de interesse do DetectorVeiculos.

// Posições dos veiculos estaticos do centro do quadrante.
std::vector<Ethernet::Position> posicoes_iniciais_centro = {
    {50, 50},   // Quadrante 1
    {-50, 50},  // Quadrante 2
    {-50, -50}, // Quadrante 3
    {50, -50}   // Quadrante 4
};

// Posições dos veiculos estaticos da borda do quadrante.
std::vector<Ethernet::Position> posicoes_iniciais_borda = {
    {50, 5},    // Quadrante 1
    {-5, 50},   // Quadrante 2
    {-50, -5},  // Quadrante 3
    {5, -50}    // Quadrante 4
};

// Índice compartilhado entre processos filhos.
int indice_posicao_global;
bool centro;

void* rotina_detector_veiculos(void* arg) {
    Veiculo::DadosComponente* dados = (Veiculo::DadosComponente*)arg;
    Communicator comunicador(dados->protocolo, dados->id_veiculo, pthread_self());

    while (true) {
        // Prepara mensagem de interesse para o sensor gps.
        Message mensagem;

        mensagem.setDstAddress({{0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, (pthread_t)0}); // Interesse externo.
        mensagem.setType(Ethernet::TYPE_POSITION_DATA);  // Preenche tipo do dado

        // Preenche periodo de interesse.
        mensagem.setPeriod(0); // Periodo = 0 => Ping (uma unica resposta).

        std::cout << "📬 " << dados->nome << ": enviou interesse." << std::endl;
        // Envia mensagem de interesse.
        comunicador.send(&mensagem);

        // Espera intervalo de envio de interesses.
        std::this_thread::sleep_for(std::chrono::milliseconds(PERIODO_DETECCAO));

        // Processa mensagens recebidas.
        while(comunicador.hasMessage()) {
            Message mensagem;
            comunicador.receive(&mensagem);
            // Verifica se a mensagem recebida é resposta (id componente eh o do componente)
            if (pthread_equal(mensagem.getDstAddress().component_id, pthread_self())) {
                // Verifica tipo de resposta recebida.
                if (mensagem.getType() == Ethernet::TYPE_POSITION_DATA) {
                    // Extrai dado recebido.
                    Ethernet::Position posicao = *reinterpret_cast<Ethernet::Position*>(mensagem.data());
                    std::cout << "📬 " << dados->nome << ": detectou veiculo na posicao: (" << posicao.x << ", " << posicao.y << ")" << std::endl;
                }
            }
        }
    }

    delete dados;
    pthread_exit(NULL);
}

void* rotina_gps_dinamico(void* arg) {
    Veiculo::DadosComponente* dados = (Veiculo::DadosComponente*)arg;
    Communicator comunicador(dados->protocolo, dados->id_veiculo, pthread_self());

    std::vector<Ethernet::Type> tipos;
    tipos.push_back(Ethernet::TYPE_POSITION_DATA);

    // Se inscreve no DataPublisher para receber mensagens de interesse nos seus tipos de dados.
    dados->data_publisher->subscribe(comunicador.getObserver(), &tipos);

    std::vector<Ethernet::Position> posicoes = {
        {50, 50}, {5, 50},      // Centro Q1, Borda Q1 com Q2
        {-50, 50}, {-50, 5},    // Centro Q2, Borda Q2 com Q3
        {-50, -50}, {-5, -50},  // Centro Q3, Borda Q3 com Q4
        {50, -50}, {50, -5}     // Centro Q4, Borda Q4 com Q1
    };

    int idx_posicao = 0;
    int num_voltas_realizadas = 0;

    while (num_voltas_realizadas < NUM_VOLTAS) {
        if (idx_posicao == 0) {
            std::cout << "Iniciou volta número: " << num_voltas_realizadas + 1 << std::endl;
        }

        Message mensagem;
        comunicador.receive(&mensagem);
        // Verifica se a mensagem é de interesse (não preencheu id componente no endereço de destino).
        if (pthread_equal(mensagem.getDstAddress().component_id, (pthread_t)0)) {
            // Responde a mensagem.
            mensagem.setDstAddress(mensagem.getSrcAddress());
            mensagem.setData(reinterpret_cast<Ethernet::Position*>(&posicoes[idx_posicao]), sizeof(Ethernet::Position));
            comunicador.send(&mensagem);
            std::cout << "\nNova Posição Veículo Dinâmico: x: " << posicoes[idx_posicao].x << ", y:" << posicoes[idx_posicao].y << std::endl;
            //std::cout << "📬 " << dados->nome << ": Enviou posição." << std::endl;
            idx_posicao++;
            if (idx_posicao == posicoes.size()) {
                num_voltas_realizadas++;
                idx_posicao = 0;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(INTERVALO_POSICAO));

            if (idx_posicao == 0) {
            std::cout << "\nTerminou volta número: " << num_voltas_realizadas << std::endl;
        }
        }
    }
    // Remove o observador do DataPublisher.
    dados->data_publisher->unsubscribe(comunicador.getObserver());
    delete dados;
    pthread_exit(NULL);
}

void* rotina_gps_estatico(void* arg) {
    Veiculo::DadosComponente* dados = (Veiculo::DadosComponente*)arg;
    Communicator comunicador(dados->protocolo, dados->id_veiculo, pthread_self());

    std::vector<Ethernet::Type> tipos;
    tipos.push_back(Ethernet::TYPE_POSITION_DATA);

    // Se inscreve no DataPublisher para receber mensagens de interesse nos seus tipos de dados.
    dados->data_publisher->subscribe(comunicador.getObserver(), &tipos);

    Ethernet::Position posicao;
    // Verifica se localizacao inicial do veiculo estatico eh centro ou borda:
    if (centro) {
        posicao = posicoes_iniciais_centro[indice_posicao_global];
    } else {
        posicao = posicoes_iniciais_borda[indice_posicao_global];
    }

    std::cout << "Posição Veículo: x: " << posicao.x << ", y:" << posicao.y << std::endl;

    int num_respostas_enviadas = 0;

    while (true) {
        if (comunicador.hasMessage()) {
            Message mensagem;
            comunicador.receive(&mensagem);

            // Verifica se a mensagem é de interesse (não preencheu id componente no endereço de destino).
            if (pthread_equal(mensagem.getDstAddress().component_id, (pthread_t)0)) {
                // Responde a mensagem.
                mensagem.setDstAddress(mensagem.getSrcAddress());
                mensagem.setData(reinterpret_cast<Ethernet::Position*>(&posicao), sizeof(Ethernet::Position));
                comunicador.send(&mensagem);
                //std::cout << "📬 " << dados->nome << ": Enviou posição." << std::endl;
                num_respostas_enviadas++;
            }
        }
    }
    // Remove o observador do DataPublisher.
    dados->data_publisher->unsubscribe(comunicador.getObserver());
    delete dados;
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

// Funcao de teste de comunicacao em grupos.
int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cout << "Erro: Por favor, informe a interface de rede.\n";
        std::cout << "Uso: " << argv[0] << " <network-interface> [intervalo_criacao] [intervalo_posicao] [num_voltas] [periodo_deteccao]\n";
        return 1;
    }

    std::string networkInterface = argv[1];

    // Função auxiliar para ler int de argv com fallback para valor default
    auto parse_arg = [&](int index, int default_val) -> int {
        if (argc > index) {
            try {
                return std::stoi(argv[index]);
            } catch (...) {
                std::cout << "Aviso: parâmetro " << index << " inválido. Usando valor padrão " << default_val << ".\n";
            }
        }
        return default_val;
    };

    INTERVALO_CRIACAO = parse_arg(2, INTERVALO_CRIACAO);
    INTERVALO_POSICAO = parse_arg(3, INTERVALO_POSICAO);
    NUM_VOLTAS = parse_arg(4, NUM_VOLTAS);
    PERIODO_DETECCAO = parse_arg(5, PERIODO_DETECCAO);

    std::cout << "\n"
              << "============================================================\n"
              << "👨‍👩‍👧‍👦  TESTE: Comunicação e Reconhecimento de Veículos em Diferentes Grupos. \n"
              << "------------------------------------------------------------\n"
              << " Cada grupo (quadrante) possui dois veículos estáticos: centro e borda.\n"
              << " Um Veículo Dinâmico circula entre todos os quadrantes no sentido anti-horário, \n"
              << " Detectando veículos do seu grupo ou vizinho ao seu grupo (área da borda), quando for o caso.\n"
              << "============================================================\n"
              << std::endl;

    // Imprime os parâmetros que serão usados no teste
    std::cout << "Parâmetros do teste:\n";
    std::cout << " Interface de rede: " << networkInterface << "\n";
    std::cout << " Intervalo criação dos veículos (ms): " << INTERVALO_CRIACAO << "\n";
    std::cout << " Intervalo troca de posição do veículo dinâmico (ms): " << INTERVALO_POSICAO << "\n";
    std::cout << " Número de voltas do veículo dinâmico : " << NUM_VOLTAS << "\n";
    std::cout << " Período de detecção de veiculos (ms): " << PERIODO_DETECCAO << "\n";

    std::vector<pid_t> pids_filhos;

    std::cout << "\nCriando RSU para cada quadrante:" << std::endl;
    RSU rsu_1(networkInterface, 1, {0, 100, 0, 100});
    RSU rsu_2(networkInterface, 2, {-100, 0, 0, 100});
    RSU rsu_3(networkInterface, 3, {-100, 0, -100, 0});
    RSU rsu_4(networkInterface, 4, {0, 100, -100, 0});

    std::this_thread::sleep_for(std::chrono::milliseconds(INTERVALO_CRIACAO));

    std::cout << "\nCriando Veículos Estáticos no Centro de cada quadrante:" << std::endl;
    // Cria processos para os veiculos estaticos do centro dos quadrantes
    for (int i = 0; i < 4; ++i) {
        std::cout << "\nCentro do Quadrante " << i+1 << std::endl;
        int idx = i;
        pid_t pid = fork();
        if (pid == 0) {
            centro = true;
            indice_posicao_global = idx;
            Veiculo veiculo(networkInterface, "Veiculo Estático Centro Q" + std::to_string(idx + 1));
            veiculo.criar_componente("GPS", rotina_gps_estatico);
            //veiculo.criar_componente("DetectorVeiculos", rotina_detector_veiculos);
            return 0;
        }
        pids_filhos.push_back(pid);
        std::this_thread::sleep_for(std::chrono::milliseconds(INTERVALO_CRIACAO));
    }

    // Cria processos para os veiculos estaticos da borda dos quadrantes.
    std::cout << "\nCriando Veículos Estáticos na Borda de cada quadrante:" << std::endl;
    for (int i = 0; i < 4; ++i) {
        std::cout << "\nBorda do Quadrante " << i+1 << std::endl;
        int idx = i;
        pid_t pid = fork();
        if (pid == 0) {
            centro = false;
            indice_posicao_global = idx;
            Veiculo veiculo(networkInterface, "Veiculo Estático Borda Q" + std::to_string(idx + 1));
            veiculo.criar_componente("GPS", rotina_gps_estatico);
            return 0;
        }
        pids_filhos.push_back(pid);
        std::this_thread::sleep_for(std::chrono::milliseconds(INTERVALO_CRIACAO));
    }

    // Cria processo para o veiculo dinamico.
    std::cout << "\nCriando Veiculo Dinamico." << std::endl;
    pid_t pid = fork();
    if (pid == 0) {
        Veiculo veiculo(networkInterface, "Veiculo Dinamico");
        veiculo.criar_componente("GPS", rotina_gps_dinamico);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        veiculo.criar_componente("DetectorVeiculos", rotina_detector_veiculos);
        return 0;
    }
    pids_filhos.push_back(pid);

    // Espera tempo de simulacao:
    // (intervalo de troca de posicao * quantidade de posicoes por volta * numero de voltas realizadas)
    std::this_thread::sleep_for(std::chrono::milliseconds(INTERVALO_POSICAO * 8 * NUM_VOLTAS));

    // Finaliza processos filhos.
    for (pid_t pid : pids_filhos) {
        kill(pid, SIGTERM);
    }

    std::cout << "\n===============================" << std::endl;
    std::cout << "✅ Teste finalizado." << std::endl;
    std::cout << "===============================\n" << std::endl;
    return 0;
}