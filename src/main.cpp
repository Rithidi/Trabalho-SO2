#include <iostream>
#include <chrono>
#include <memory>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "../include/communicator.hpp"
#include "../include/message.hpp"
#include "../include/nic.hpp"
#include "../include/protocol.hpp"
#include "../include/engine.hpp"

using namespace std;
using namespace std::chrono_literals;

// Endereços MAC fictícios para os veículos
const Ethernet::Mac_Address MAC_VEICULO_A = {0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0x01};
const Ethernet::Mac_Address MAC_VEICULO_B = {0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0x02};

// Portas para os componentes
const Ethernet::Port PORTA_SENSOR = 5000;
const Ethernet::Port PORTA_CONTROLE = 5001;

// Função que simula o envio de mensagens pelo sensor
void enviar_mensagem() {
    // Criar NICs e protocolos para os veículos
    NIC<Engine> nic_a("eno1");
    nic_a.set_address(MAC_VEICULO_A);
    Protocol protocol_a(&nic_a, 0x88B5);
    Communicator sensor(&protocol_a, MAC_VEICULO_A, PORTA_SENSOR);

    int contador = 1;

    while (contador < 10001) { // Enviar 5 mensagens para teste
        Message msg;
        string dados = "Mensagem " + to_string(contador);
        msg.setData(dados.c_str(), dados.size() + 1);

        contador++;

        // Endereço de destino (componente de controle do outro veículo)
        Ethernet::Address destino = {MAC_VEICULO_B, PORTA_CONTROLE};

        // Enviar mensagem
        if (sensor.send(&msg, destino)) {
            cout << "Sensor (veiculo 1)" << " enviou: " << dados << endl;

            // Envia a confirmação para o processo filho via pipe
        } else {
            cerr << "Falha no envio do sensor" << endl;
        }
        this_thread::sleep_for(0.0001s); // Espera 1 segundo entre os envios
    }
}

// Função que simula o recebimento de mensagens pelo controle
void receber_mensagem() {
    // Criar NICs e protocolos para os veículos
    NIC<Engine> nic_b("eno1");
    nic_b.set_address(MAC_VEICULO_B);
    Protocol protocol_b(&nic_b, 0x88B5);
    Communicator controle(&protocol_b, MAC_VEICULO_B, PORTA_CONTROLE);

    for (int i = 0; i < 10000; ++i) { // Esperar 5 mensagens para teste
        Message msg;

        if (controle.receive(&msg)) {
            string mensagem(reinterpret_cast<const char*>(msg.data()));
            cout << i + 1 << "Controle (veiculo 2)" << " recebeu: " << mensagem << endl;
        } else {
            cerr << "Falha no recebimento do controle" << endl;
        }
    }
}

int main() {
    cout << "TESTE: Comunicacao entre componentes de veiculos diferentes (processos distintos)" << endl;
    pid_t pid = fork(); // Cria o processo filho

    if (pid == -1) {
        cerr << "Erro ao criar o processo" << endl;
        return 1;
    }

    if (pid > 0) {
        // Processo pai - Enviar mensagens
        this_thread::sleep_for(0.1s); // Espera processo de recebimento iniciar NIC.
        enviar_mensagem();
        wait(NULL); // Espera o processo filho terminar
        cout << "Processo enviar terminou." << endl;
    } else {
        // Processo filho - Receber mensagens
        receber_mensagem();
        cout << "Processo receber terminou." << endl;
    }

    cout << "Teste finalizado." << endl;
    return 0;
}
