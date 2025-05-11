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

// Endereços MAC fictícios para o veiculo receptor
const Ethernet::Mac_Address MAC_VEICULO_A = {0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0x01};
// Endereços MAC fictícios para os veículos enviando mensagens
const Ethernet::Mac_Address MAC_VEICULO_B = {0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0x02};
const Ethernet::Mac_Address MAC_VEICULO_C = {0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0x03};
const Ethernet::Mac_Address MAC_VEICULO_D = {0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0x04};

const int NUM_VEICULOS_ENVIANDO = 3; // Número de veículos enviando mensagens

// Portas para os componentes
const Ethernet::Port PORTA_SENSOR = 5000;
const Ethernet::Port PORTA_CONTROLE = 5001;

// Macro auxiliar para transformar a macro NETWORK_INTERFACE em uma string literal
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

// Definir o número total de mensagens como uma macro
#ifndef TOTAL_MESSAGES
#define TOTAL_MESSAGES 500 // Valor padrão caso a macro não seja definida
#endif

// Função que simula o envio de mensagens pelo sensor
void enviar_mensagem(Ethernet::Mac_Address mac, string nome_veiculo){
    const char* interface = 
#ifdef NETWORK_INTERFACE
        TOSTRING(NETWORK_INTERFACE);
#else
        "lo";
#endif


    NIC<Engine> nic_a(interface);
    nic_a.set_address(mac);
    Protocol protocol_a(&nic_a, 0x88B5);
    Communicator sensor(&protocol_a, mac, PORTA_SENSOR);

    int contador = 1;

    while (contador <= TOTAL_MESSAGES) {
        Message msg;

        // Add timestamp to the message
        auto now = duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
        string dados = "Mensagem de " + nome_veiculo + " Num: " + to_string(contador) + " |" + to_string(now);
        msg.setData(dados.c_str(), dados.size() + 1);

        contador++;

        Ethernet::Address destino = {MAC_VEICULO_A, PORTA_CONTROLE};

        // Envia mensagem para o controle do veículo A
        if (sensor.send(&msg, destino)) {
            cout << nome_veiculo << " enviou: " << dados << endl;
        } else {
            cerr << "Falha no envio do sensor" << endl;
        }

        // Adiciona pequeno delay entre os envios para evitar sobrecarga
        this_thread::sleep_for(1ms); // Ajuste o delay conforme necessário
    }
}

// Função que simula o recebimento de mensagens pelo controle
void receber_mensagem(Ethernet::Mac_Address mac, string nome_veiculo) {
    const char* interface = 
#ifdef NETWORK_INTERFACE
        TOSTRING(NETWORK_INTERFACE);
#else
        "lo";
#endif

    NIC<Engine> nic_b(interface);
    nic_b.set_address(mac);
    Protocol protocol_b(&nic_b, 0x88B5);
    Communicator controle(&protocol_b, mac, PORTA_CONTROLE);

    int i = 0; // Contador de mensagens recebidas
    double total_latency = 0.0;

    while (i < TOTAL_MESSAGES * NUM_VEICULOS_ENVIANDO) { // Ajusta o loop para o número total de mensagens
        Message msg;

        // Espera por mensagens dos sensores de outros veículos (B, C, D)
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

                cout << nome_veiculo << " recebeu: " << mensagem.substr(0, pos) 
                     << ", Latência: " << latency << " ms, Total: " << i + 1 << endl;
            }
            i++;
        } else {
            cerr << "Falha no recebimento do controle" << endl;
        }
    }
    // Exibe o total de mensagens recebidas e a latência média
    cout << "\nTotal de mensagens recebidas pelo controle do " << nome_veiculo << ": " << i << endl;
    cout << "Latência média: " << (total_latency / i) << " ms" << endl;
}

int main() {
    cout << "\nTESTE: Comunicação entre componentes de veículos diferentes (processos distintos)" << endl;
    cout << "# Sensores dos veículos: B, C, D, enviam mensagens para o componente controle do veículo A\n" << endl;
    cout << "# sensor B -envia-> controle A" << endl;
    cout << "# sensor C -envia-> controle A" << endl;
    cout << "# sensor D -envia-> controle A\n" << endl;

    pid_t pid_recebedor = fork();

    if (pid_recebedor == -1) {
        cerr << "Erro ao criar o processo do receptor" << endl;
        return 1;
    }

    if (pid_recebedor == 0) {
        // Processo filho: receptor (Veículo A)
        receber_mensagem(MAC_VEICULO_A, "Veículo A");
        return 0;
    }

    // Pequeno delay para garantir que o receptor esteja pronto
    this_thread::sleep_for(100ms);

    // Criar processo para Veículo B
    pid_t pid_b = fork();

    if (pid_b == 0) {
        enviar_mensagem(MAC_VEICULO_B, "Veículo B");
        return 0;
    }

    // Criar processo para Veículo C
    pid_t pid_c = fork();

    if (pid_c == 0) {
        enviar_mensagem(MAC_VEICULO_C, "Veículo C");
        return 0;
    }

    // Criar processo para Veículo D
    pid_t pid_d = fork();

    if (pid_d == 0) {
        enviar_mensagem(MAC_VEICULO_D, "Veículo D");
        return 0;
    }

    // Processo pai espera todos os filhos
    while (wait(nullptr) > 0); // Espera todos os filhos terminarem

    return 0;
}
