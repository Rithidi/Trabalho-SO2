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
using namespace std::chrono;

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
    const char* interface = 
#ifdef NETWORK_INTERFACE
        TOSTRING(NETWORK_INTERFACE);
#else
        "lo";
#endif

    NIC<Engine> nic_a(interface);
    nic_a.set_address(MAC_VEICULO_A);
    Protocol protocol_a(&nic_a, 0x88B5);
    Communicator sensor(&protocol_a, MAC_VEICULO_A, PORTA_SENSOR);

    int contador = 1;

    while (contador <= TOTAL_MESSAGES) {
        Message msg;

        // Add timestamp to the message
        auto now = duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
        string dados = "Mensagem N:" + to_string(contador) + "|" + to_string(now);
        msg.setData(dados.c_str(), dados.size() + 1);

        contador++;

        Ethernet::Address destino = {MAC_VEICULO_B, PORTA_CONTROLE};

        if (sensor.send(&msg, destino)) {
            cout << "Veiculo 1 enviou: " << dados << endl;
        } else {
            cerr << "Falha no envio do sensor" << endl;
        }
    }
}

// Função que simula o recebimento de mensagens pelo controle
void receber_mensagem() {
    const char* interface = 
#ifdef NETWORK_INTERFACE
        TOSTRING(NETWORK_INTERFACE);
#else
        "lo";
#endif

    NIC<Engine> nic_b(interface);
    nic_b.set_address(MAC_VEICULO_B);
    Protocol protocol_b(&nic_b, 0x88B5);
    Communicator controle(&protocol_b, MAC_VEICULO_B, PORTA_CONTROLE);

    int i = 1;
    double total_latency = 0.0;

    while (i <= TOTAL_MESSAGES) {
        Message msg;

        if (controle.receive(&msg)) {
            string mensagem(reinterpret_cast<const char*>(msg.data()));

            // Extract timestamp from the message
            size_t pos = mensagem.find_last_of('|');
            if (pos != string::npos) {
                string timestamp_str = mensagem.substr(pos + 1);
                auto sent_time = stoll(timestamp_str);

                // Calculate latency
                auto now = duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
                double latency = (now - sent_time) / 1000.0; // Convert to milliseconds
                total_latency += latency;

                cout << "Veiculo 2 recebeu: " << mensagem.substr(0, pos) 
                     << ", Latencia: " << latency << " ms, Total: " << i << endl;
            }

            i++;
        } else {
            cerr << "Falha no recebimento do controle" << endl;
        }
    }

    // Exibe o total de mensagens recebidas e a latência média
    cout << "Total de mensagens recebidas pelo controle: " << TOTAL_MESSAGES << endl;
    cout << "Latência média: " << (total_latency / TOTAL_MESSAGES) << " ms" << endl;
}

int main() {
    cout << "TESTE: Comunicacao entre componentes de veiculos diferentes (processos distintos)" << endl;
    pid_t pid = fork();

    if (pid == -1) {
        cerr << "Erro ao criar o processo" << endl;
        return 1;
    }

    if (pid > 0) {
        this_thread::sleep_for(2ms); 
        enviar_mensagem();
        wait(nullptr);
    } else {
        receber_mensagem();
    }

    return 0;
}