#include <iostream>
#include <thread>
#include "../include/ethernet.hpp"
#include "../include/nic.hpp"
#include "../include/protocol.hpp"
#include "../include/communicator.hpp"
#include "../include/message.hpp"

// Componente do carro (ex: sensor)
class Sensor {
public:
    Sensor(const std::string& nome, Communicator<Protocol<NIC<Engine>>>* comm) 
        : nome(nome), comm(comm) {}

    void enviar_dados() {
        for(int i = 0; i < 3; i++) {
            std::string dados = nome + " - Leitura " + std::to_string(i);
            Message msg;
            msg.setData(dados.c_str(), dados.size());
            
            if(comm->send(&msg)) {
                std::cout << nome << " enviou: " << dados << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

private:
    std::string nome;
    Communicator<Protocol<NIC<Engine>>>* comm;
};

// Componente do carro (ex: módulo de controle)
class ControlModule {
public:
    ControlModule(const std::string& nome, Communicator<Protocol<NIC<Engine>>>* comm) 
        : nome(nome), comm(comm), running(true) {}

    void iniciar() {
        thread = std::thread(&ControlModule::receber_dados, this);
    }

    void parar() {
        running = false;
        if(thread.joinable()) thread.join();
    }

    void receber_dados() {
        while(running) {
            Message msg;
            if(comm->receive(&msg)) {
                std::string dados(static_cast<const char*>(msg.data()), msg.size());
                std::cout << nome << " recebeu: " << dados << std::endl;
            }
        }
    }

private:
    std::string nome;
    Communicator<Protocol<NIC<Engine>>>* comm;
    std::thread thread;
    bool running;
};

// Simulação de comunicação no mesmo carro
void simulacao_mesmo_carro() {
    std::cout << "\n=== COMUNICAÇÃO NO MESMO CARRO ===\n";
    
    // 1. Cria a infraestrutura de rede do carro
    NIC<Engine> nic;
    Protocol<NIC<Engine>> protocol(&nic);
    
    // 2. Define endereços dos componentes (mesmo MAC, portas diferentes)
    Ethernet::Address mac_carro = {0x00, 0x0C, 0x29, 0x12, 0x34, 0x56};
    nic.set_address(mac_carro);
    
    // 3. Cria comunicadores
    Communicator<Protocol<NIC<Engine>>> comm_sensor(&protocol, 
        Protocol<NIC<Engine>>::Address(mac_carro, 1000));
    
    Communicator<Protocol<NIC<Engine>>> comm_controle(&protocol, 
        Protocol<NIC<Engine>>::Address(mac_carro, 2000));
    
    // 4. Cria componentes
    Sensor sensor("Sensor Freio", &comm_sensor);
    ControlModule controle("Controle ABS", &comm_controle);
    
    // 5. Inicia comunicação
    controle.iniciar();
    sensor.enviar_dados();
    
    // 6. Finaliza
    controle.parar();
}


// Simulação de comunicação entre carros diferentes
void simulacao_carros_diferentes() {
    std::cout << "\n=== COMUNICAÇÃO ENTRE CARROS ===\n";
    
    // 1. Cria a infraestrutura para dois carros
    NIC<Engine> nic_carro1, nic_carro2;
    
    // 2. Define MACs únicos para cada carro
    Ethernet::Address mac_carro1 = {0x00, 0x0C, 0x29, 0x11, 0x11, 0x11};
    Ethernet::Address mac_carro2 = {0x00, 0x0C, 0x29, 0x22, 0x22, 0x22};
    
    nic_carro1.set_address(mac_carro1);
    nic_carro2.set_address(mac_carro2);
    
    // 3. Cria protocolos e comunicadores
    Protocol<NIC<Engine>> protocol_carro1(&nic_carro1), protocol_carro2(&nic_carro2);
    
    // Comunicador do carro 1 (envia broadcast)
    Communicator<Protocol<NIC<Engine>>> comm_carro1(&protocol_carro1, 
        Protocol<NIC<Engine>>::Address(mac_carro1, 1000));
    
    // Comunicador do carro 2 (escuta broadcast)
    Communicator<Protocol<NIC<Engine>>> comm_carro2(&protocol_carro2, 
        Protocol<NIC<Engine>>::Address(mac_carro2, 1000));
    
    // 4. Cria e executa componentes
    ControlModule controle_carro2("Carro2-Controle", &comm_carro2);
    controle_carro2.iniciar();
    
    // Carro 1 envia mensagem broadcast
    Message msg;
    msg.setData("Emergência: Freada brusca!", 24);
    comm_carro1.send(&msg);
    std::cout << "Carro 1 enviou mensagem de emergência\n";
    
    // Aguarda processamento
    std::this_thread::sleep_for(std::chrono::seconds(2));
    controle_carro2.parar();
}


int main() {
    // Simula comunicação interna no mesmo veículo
    simulacao_mesmo_carro();
    
    // Simula comunicação V2V (veículo para veículo)
    simulacao_carros_diferentes();
    
    return 0;
}