#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <vector>
#include <atomic>
#include "../include/communicator.hpp"
#include "../include/message.hpp"
#include "../include/nic.hpp"
#include "../include/protocol.hpp"
#include "../include/engine.hpp"



using namespace std;
using namespace std::chrono_literals;

// Endereços MAC fictícios para os veículos
const array<uint8_t, 6> MAC_VEICULO_A = {0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0x01};
const array<uint8_t, 6> MAC_VEICULO_B = {0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0x02};

// Portas para os componentes
const Ethernet::Port PORTA_SENSOR = 5000;
const Ethernet::Port PORTA_CONTROLE = 5001;

// Função que simula um componente sensor de um veículo
void componente_sensor(shared_ptr<Protocol> protocol, const array<uint8_t, 6>& mac_veiculo) {
    Communicator sensor(protocol.get(), mac_veiculo, PORTA_SENSOR);
    
    int contador = 0;
    while (true) {
        // Criar mensagem com dados do sensor
        Message msg;
        string dados = "DadosSensor#" + to_string(contador++) + " do veiculo " + 
                      to_string(mac_veiculo[5]);
        msg.setData(dados.c_str(), dados.size() + 1);
        
        // Endereço de destino (broadcast para o componente de controle do outro veículo)
        Ethernet::Address destino = {MAC_VEICULO_B, PORTA_CONTROLE};
        
        // Enviar mensagem
        if (sensor.send(&msg, destino)) {
            cout << "Sensor (veiculo " << (int)mac_veiculo[5] 
                 << ") enviou: " << dados << endl;
        } else {
            cerr << "Falha no envio do sensor" << endl;
        }
        
        this_thread::sleep_for(2s);
    }
}

// Função que simula um componente de controle de um veículo
void componente_controle(shared_ptr<Protocol> protocol, const array<uint8_t, 6>& mac_veiculo) {
    Communicator controle(protocol.get(), mac_veiculo, PORTA_CONTROLE);
    
    while (true) {
        Message msg;
        if (controle.receive(&msg)) {
            string mensagem(reinterpret_cast<const char*>(msg.data()));
            cout << "Controle (veiculo " << (int)mac_veiculo[5] 
                 << ") recebeu: " << mensagem << endl;
            
            // Processar mensagem e tomar decisões...
        }
        this_thread::sleep_for(100ms);
    }
}

// Função principal que simula um veículo autônomo
void simular_veiculo(const string& nome, const array<uint8_t, 6>& mac) {
    cout << "Iniciando " << nome << " com MAC: ";
    for (auto b : mac) printf("%02X:", b);
    cout << endl;
    
    // Criar NIC e protocolo para o veículo
    auto nic = make_shared<NIC<Engine>>("lo");
    nic->set_address(mac);
    auto protocol = make_shared<Protocol>(nic.get(), 0x88B5);
    
    // Criar threads para os componentes
    vector<thread> threads;
    
    // Componente sensor
    threads.emplace_back(componente_sensor, protocol, mac);
    
    // Componente de controle
    threads.emplace_back(componente_controle, protocol, mac);
    
    // Manter o veículo ativo
    while (true) {
        this_thread::sleep_for(10s);
    }
    
    // Nunca vai chegar aqui neste exemplo
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }
}

int main() {
    cout << "Simulacao de comunicacao entre veiculos autonomos" << endl;
    cout << "Cada veiculo tem componentes (threads) que se comunicam" << endl;
    
    // Criar threads para cada veículo (em um sistema real seriam processos separados)
    thread veiculo_a(simular_veiculo, "Veiculo A", MAC_VEICULO_A);
    thread veiculo_b(simular_veiculo, "Veiculo B", MAC_VEICULO_B);
    
    // Manter o teste rodando por um tempo
    this_thread::sleep_for(30s);
    
    cout << "Finalizando simulacao..." << endl;
    
    // Encerrar threads (nota: em uma aplicação real precisaria de um mecanismo de shutdown adequado)
    veiculo_a.detach();
    veiculo_b.detach();
    
    return 0;
}