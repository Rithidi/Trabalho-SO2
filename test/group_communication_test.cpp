#include "../include/rsu.hpp"
#include "../include/vehicle.hpp"

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <iostream>

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

void* rotina_gps_dinamico(void* arg);

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

    while (num_respostas_enviadas < 5) {
        Message mensagem;
        comunicador.receive(&mensagem);

        // Verifica se a mensagem é de interesse (não preencheu id componente no endereço de destino).
        if (pthread_equal(mensagem.getDstAddress().component_id, (pthread_t)0)) {
            // Responde a mensagem.
            mensagem.setDstAddress(mensagem.getSrcAddress());
            mensagem.setData(reinterpret_cast<Ethernet::Position*>(&posicao), sizeof(Ethernet::Position));
            comunicador.send(&mensagem);
            std::cout << "📬 " << dados->nome << ": Enviou posição." << std::endl;
            num_respostas_enviadas++;
        }
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    // Remove o observador do DataPublisher.
    dados->data_publisher->unsubscribe(comunicador.getObserver());
    
    std::cout << "GPS FINALIZOU" << std::endl;

    delete dados;
    pthread_exit(NULL);
}

int main() {
    std::cout << "\nCriando RSU para cada quadrante:" << std::endl;
    RSU rsu_1("enp0s1", 1, {0, 100, 0, 100});
    RSU rsu_2("enp0s1", 2, {-100, 0, 0, 100});
    RSU rsu_3("enp0s1", 3, {-100, 0, -100, 0});
    RSU rsu_4("enp0s1", 4, {0, 100, -100, 0});

    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::cout << "PID DO PROCESSO PAI: " << getppid() << std::endl;

    std::cout << "\nCriando Veiculo estatico no centro de cada quadrante:" << std::endl;
    // Cria processos para os veiculos estaticos do centro dos quadrantes
    for (int i = 0; i < 4; ++i) {
        std::cout << "\nQuadrante " << i+1 << std::endl;
        int idx = i;
        pid_t pid = fork();
        if (pid == 0) {
            centro = true;
            indice_posicao_global = idx;
            Veiculo veiculo("enp0s1", "Veiculo Estático Centro Q" + std::to_string(idx + 1));
            veiculo.criar_componente("GPS", rotina_gps_estatico);
            return 0;
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    std::cout << "\nCriando Veiculo estatico na borda de cada quadrante:" << std::endl;
    // Cria processos para os veiculos estaticos da borda dos quadrantes.
    for (int i = 0; i < 4; ++i) {
        std::cout << "\nQuadrante " << i+1 << std::endl;
        int idx = i;
        pid_t pid = fork();
        if (pid == 0) {
            centro = false;
            indice_posicao_global = idx;
            Veiculo veiculo("enp0s1", "Veiculo Estático Borda Q" + std::to_string(idx + 1));
            veiculo.criar_componente("GPS", rotina_gps_estatico);
            return 0;
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    // Após criar todos os filhos, imprime os PIDs deles
    std::cout << "\nProcesso pai (PID " << getpid() << ") criou os seguintes filhos:\n";
    for (pid_t pid : filhos) {
        std::cout << " - Filho com PID: " << pid << std::endl;
    }

    // Espera todos os processos filhos terminarem
    while (wait(NULL) > 0);

    std::cout << "PROCESSO PAI TERMINOU." << std::endl;

    //std::this_thread::sleep_for(std::chrono::seconds(30));

    return 0;
}