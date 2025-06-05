#include "../include/rsu.hpp"
#include "../include/vehicle.hpp"

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <iostream>

void* rotina_gps_dinamico(void* arg);

void* rotina_gps_estatico(void* arg) {
    Veiculo::DadosComponente* dados = (Veiculo::DadosComponente*)arg;
    Communicator comunicador(dados->protocolo, dados->id_veiculo, pthread_self());

    std::vector<Ethernet::Type> tipos;
    tipos.push_back(Ethernet::TYPE_POSITION_DATA);

    // Se inscreve no DataPublisher para receber mensagens de interesse nos seus tipos de dados.
    dados->data_publisher->subscribe(comunicador.getObserver(), &tipos);

    Ethernet::Position posicao = {-50, 0}; // Posição inicial do veículo

    int num_respostas_enviadas = 0;

    while (num_respostas_enviadas < 10) {
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
    }
    // Remove o observador do DataPublisher.
    dados->data_publisher->unsubscribe(comunicador.getObserver());
        
    delete dados;
    pthread_exit(NULL);
}

int main() {
    RSU rsu_1("enp0s1", 1, {0, 100, 0, 100});
    RSU rsu_2("enp0s1", 2, {-100, 0, 0, 100});
    //RSU rsu_3("enp0s1", 3, {-100, 0, -100, 0});
    //RSU rsu_4("enp0s1", 4, {0, 100, -100, 0});

    // Cria Processo.
    pid_t pid = fork();

    if (pid == 0) {
        // Cria Veículo 1.
        Veiculo veiculo("enp0s1", "Veiculo 1");
        veiculo.criar_componente("GPS", rotina_gps_estatico);
        std::this_thread::sleep_for(std::chrono::seconds(20));
        return 0;
    }

    // Espera todos os processos filhos terminarem
    while (wait(NULL) > 0);

    std::this_thread::sleep_for(std::chrono::seconds(30));

    return 0;
}