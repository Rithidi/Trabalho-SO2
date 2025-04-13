#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
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
void enviar_mensagem(Communicator* sensor) {
    int contador = 1;

    while (contador < 6) { // Enviar 5 mensagens para teste
        Message msg;
        string dados = " Mensagem " + to_string(contador);
        msg.setData(dados.c_str(), dados.size() + 1);

        contador++;

        // Endereço de destino (componente de controle do outro veículo)
        Ethernet::Address destino = {MAC_VEICULO_B, PORTA_CONTROLE};

        // Enviar mensagem
        if (sensor->send(&msg, destino)) {
            cout << "Sensor (veiculo 1)" << " enviou: " << dados << endl;
            this_thread::sleep_for(500ms);
        } else {
            cerr << "Falha no envio do sensor" << endl;
        }
    }
}

// Função que simula o recebimento de mensagens pelo controle
void receber_mensagem(Communicator* controle) {
    for (int i = 0; i < 5; ++i) { // Esperar 5 mensagens para teste
        Message msg;

        if (controle->receive(&msg)) {
            string mensagem(reinterpret_cast<const char*>(msg.data()));
            cout << "Controle (veiculo 2)" << " recebeu: " << mensagem << endl;
        } else {
            cerr << "Falha no recebimento do controle" << endl;
        }
    }
}

int main() {
    cout << "Teste simplificado de comunicacao entre veiculos (threads no mesmo processo)" << endl;

    // Criar NICs e protocolos para os veículos
    NIC<Engine> nic_a("eno1");
    nic_a.set_address(MAC_VEICULO_A);
    Protocol protocol_a(&nic_a, 0x88B5);
    Communicator sensor(&protocol_a, MAC_VEICULO_A, PORTA_SENSOR);

    NIC<Engine> nic_b("eno1");
    nic_b.set_address(MAC_VEICULO_B);
    Protocol protocol_b(&nic_b, 0x88B5);
    Communicator controle(&protocol_b, MAC_VEICULO_B, PORTA_CONTROLE);

    // Criar threads para enviar e receber mensagens
    thread thread_sensor(enviar_mensagem, &sensor);
    thread thread_controle(receber_mensagem, &controle);

    // Esperar as threads terminarem
    thread_sensor.join();
    thread_controle.join();

    cout << "Teste finalizado." << endl;
    return 0;
}

