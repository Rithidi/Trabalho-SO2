// Inclusão dos cabeçalhos necessários
#include "../include/communicator.hpp"  // Comunicador entre componentes
#include "../include/protocol.hpp"     // Protocolo de comunicação
#include "../include/nic.hpp"          // Interface de rede
#include "../include/engine.hpp"       // Motor de comunicação de baixo nível
#include "../include/message.hpp"      // Estrutura de mensagem
#include <iostream>                    // Para entrada/saída
#include <thread>                      // Para trabalhar com threads
#include <cstring>                     // Para funções de manipulação de strings

using namespace std;

// Função para o carro transmissor
void carro_transmissor() {
    // Cria a interface de rede do carro transmissor
    NIC<Engine> nic;
    
    // Configura o protocolo de comunicação usando a interface de rede
    Protocol<NIC<Engine>> protocol(&nic);
    
    // Cria o comunicador para a porta 8000 usando o endereço da NIC
    Communicator<Protocol<NIC<Engine>>> comm(&protocol, 
        Protocol<NIC<Engine>>::Address(nic.get_address(), 8000));

    cout << "[TRANSMISSOR] Preparado para enviar..." << endl;

    // Espera 1 segundo para garantir que o receptor esteja pronto
    this_thread::sleep_for(chrono::seconds(1));

    // Prepara a mensagem a ser enviada
    Message msg;
    const char* mensagem = "oi carro";  // Mensagem fixa de teste
    msg.setData(mensagem, strlen(mensagem) + 1);  // +1 para incluir o '\0'

    // Envia a mensagem e verifica se foi bem sucedido
    if (comm.send(&msg)) {
        cout << "[TRANSMISSOR] Mensagem enviada: \"" << mensagem << "\"" << endl;
    } else {
        cerr << "[TRANSMISSOR] Erro ao enviar mensagem" << endl;
    }
}

// Função para o carro receptor
void carro_receptor() {
    // Cria a interface de rede do carro receptor
    NIC<Engine> nic;
    
    // Configura o protocolo de comunicação usando a interface de rede
    Protocol<NIC<Engine>> protocol(&nic);
    
    // Cria o comunicador para a porta 8000 usando o endereço da NIC
    Communicator<Protocol<NIC<Engine>>> comm(&protocol, 
        Protocol<NIC<Engine>>::Address(nic.get_address(), 8000));

    cout << "[RECEPTOR] Aguardando mensagem..." << endl;

    // Variável para armazenar a mensagem recebida
    Message received;
    
    // Tenta receber a mensagem (operção bloqueante)
    if (comm.receive(&received)) {
        cout << "[RECEPTOR] Mensagem recebida: \"" << (const char*)received.data() << "\"" << endl;
    } else {
        cerr << "[RECEPTOR] Erro ao receber mensagem" << endl;
    }
}

int main() {
    // Cria duas threads para simular os dois carros:
    // - Uma thread para o carro transmissor
    // - Outra thread para o carro receptor
    thread transmitter(carro_transmissor);
    thread receiver(carro_receptor);

    // Aguarda ambas as threads terminarem sua execução
    // (join() bloqueia até a thread completar)
    transmitter.join();
    receiver.join();

    return 0;  // Fim do programa
}