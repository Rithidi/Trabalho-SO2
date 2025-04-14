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

// Macro auxiliar para transformar a macro NETWORK_INTERFACE em uma string literal
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

// Definir o número total de mensagens como uma macro
#ifndef TOTAL_MESSAGES
#define TOTAL_MESSAGES 100 // Valor padrão caso a macro não seja definida
#endif

// Função que simula o envio de mensagens pelo sensor
void enviar_mensagem() {
    // Criar NICs e protocolos para os veículos
    const char* interface = 
#ifdef NETWORK_INTERFACE
        TOSTRING(NETWORK_INTERFACE); // Transforma a macro em string literal
#else
        "lo"; // Valor padrão caso a macro não seja definida
#endif

    NIC<Engine> nic_a(interface);
    nic_a.set_address(MAC_VEICULO_A);
    Protocol protocol_a(&nic_a, 0x88B5);
    Communicator sensor(&protocol_a, MAC_VEICULO_A, PORTA_SENSOR);

    int contador = 1;

    while (contador <= TOTAL_MESSAGES) { // Enviar o número total de mensagens configurado
        Message msg;
        string dados = "Mensagem N:" + to_string(contador);
        msg.setData(dados.c_str(), dados.size() + 1);

        contador++;

        // Endereço de destino (componente de controle do outro veículo)
        Ethernet::Address destino = {MAC_VEICULO_B, PORTA_CONTROLE};

        // Enviar mensagem
        if (sensor.send(&msg, destino)) {
            cout << "Veiculo 1 enviou: " << dados << endl;
        } else {
            cerr << "Falha no envio do sensor" << endl;
        }
    }
}

// Função que simula o recebimento de mensagens pelo controle
void receber_mensagem() {
    // Criar NICs e protocolos para os veículos
    const char* interface = 
#ifdef NETWORK_INTERFACE
        TOSTRING(NETWORK_INTERFACE); // Transforma a macro em string literal
#else
        "lo"; // Valor padrão caso a macro não seja definida
#endif

    NIC<Engine> nic_b(interface);
    nic_b.set_address(MAC_VEICULO_B);
    Protocol protocol_b(&nic_b, 0x88B5);
    Communicator controle(&protocol_b, MAC_VEICULO_B, PORTA_CONTROLE);

    int i = 1;
    while (i <= TOTAL_MESSAGES) { // Receber o número total de mensagens configurado
        Message msg;

        if (controle.receive(&msg)) {
            string mensagem(reinterpret_cast<const char*>(msg.data()));
            cout << "Veiculo 2 recebeu: " << mensagem << ", Total: " << i << endl;
            i++;
        } else {
            cerr << "Falha no recebimento do controle" << endl;
        }
    }

    // Exibe o total de mensagens recebidas
    cout << "Total de mensagens recebidas pelo controle: " << TOTAL_MESSAGES << endl;
}

int main() {
    cout << "TESTE: Comunicacao entre componentes de veiculos diferentes (processos distintos)" << endl;
    pid_t pid = fork(); // Cria o processo filho

    if (pid == -1) {
        cerr << "Erro ao criar o processo" << endl;
        return 1;
    }

    if (pid > 0) {
        this_thread::sleep_for(2ms); 
        enviar_mensagem();
        wait(nullptr); // Espera o processo filho terminar
    } else {
        receber_mensagem();
    }

    return 0;
}
