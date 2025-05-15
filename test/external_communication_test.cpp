#include "../include/communicator.hpp"
#include "../include/message.hpp"
#include "../include/nic.hpp"
#include "../include/protocol.hpp"
#include "../include/engine.hpp"

#include "../test/vehicle.hpp"

#include <string>
#include <pthread.h>
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>

// Definindo identificadores para os tipos de dados.
const unsigned short DETECTOR_VIZINHOS = 0x0001;
const unsigned short GPS = 0x0002;

// Estrutura de dados que o componente GPS fornce.
struct DadosGPS {
    double latitude;
    double longitude;
};

// Funcao de rotina executada pela thread: componente DetectorVizinhos.
void* rotina_detector_vizinhos(void* arg) {
    // Converte o argumento recebido para o tipo apropriado.
    Veiculo::DadosComponente* dados = (Veiculo::DadosComponente*)arg;

    // Cria e inicializa Communicator.
    Communicator comunicador(dados->protocolo, dados->id_veiculo, pthread_self(), dados->porta);

    // Intancia de mensagem.
    Message mensagem;

    // Envia mensagem de Interesse para os componentes GPS de veiculos diferentes (comunicacao externa).
    int periodo = 1000; // Periodo de envio em milissegundos.
    // Preenche mensagem com periodo de envio.
    mensagem.setData(reinterpret_cast<char*>(&periodo), sizeof(int));
    // Preenche endereco de destino apenas com a Porta referente ao componente GPS.
    Ethernet::Address endereco_destino;
    endereco_destino.port = GPS;
    // Envia mensagem.
    comunicador.send(&mensagem, endereco_destino);
    std::cout << "📬 " << dados->nome << ": enviou interesse para os GPSs ao redor." << std::endl;

    while (true) {
        Ethernet::Address origem;
        // Cria estrutura de dados GPS para receber dados.
        DadosGPS dado;
        // Espera recebimento de mensagens dos componentes GPS.
        comunicador.receive(&mensagem, &origem);

        // Identifica o tipo de dado recebido (através da porta de origem).
        if (origem.port == GPS) {
            // Extrai dados da mensagem recebida.
            memcpy(&dado, mensagem.data(), sizeof(DadosGPS));
            std::cout << "📬 " << dados->nome << ": recebeu posicao: (" << dado.latitude << ", " << dado.longitude << ")" << std::endl;
        }
        //break;
    }
    Veiculo::DadosComponente* typedArg = static_cast<Veiculo::DadosComponente*>(arg);
    delete typedArg;
    pthread_exit(NULL);
}

// Funcao de rotina executada pela thread: componente GPS.
void* rotina_gps(void* arg) {
    // Converte o argumento recebido para o tipo apropriado.
    Veiculo::DadosComponente* dados = (Veiculo::DadosComponente*)arg;

    // Cria e inicializa Communicator.
    Communicator comunicador(dados->protocolo, dados->id_veiculo, pthread_self(), dados->porta);

    // Cria estrutura de dados do GPS.
    DadosGPS posicao{0.0, 0.0};

    // Espera receber mensagem de interesse de algum componente.
    Message mensagem;
    Ethernet::Address endereco_interessado;
    comunicador.receive(&mensagem, &endereco_interessado);

    // Identifica o tipo do interessado (através da porta de origem).
    if (endereco_interessado.port == DETECTOR_VIZINHOS) {
        std::cout << "📬 " << dados->nome << ": recebeu interesse do detector de vizinhos." << std::endl;
    }

    // Extrai periodo de envio da mensagem recebida.
    int periodo = *reinterpret_cast<int*>(mensagem.data());
    
    // Envia periodicamente seus dados para o solicitante.
    while (true) {
        // Espera o periodo definido.
        std::this_thread::sleep_for(std::chrono::milliseconds(periodo));
        // Preenche mensagem com dados de posicao do GPS.
        mensagem.setData(reinterpret_cast<char*>(&posicao), sizeof(DadosGPS));
        
        // Envia mensagem com dados de posicao.
        comunicador.send(&mensagem, endereco_interessado);

        // Incrementa dados para o próximo envio.
        posicao.latitude += 0.1;
        posicao.longitude += 0.1;
        //break;
    }
    Veiculo::DadosComponente* typedArg = static_cast<Veiculo::DadosComponente*>(arg);
    delete typedArg;
    pthread_exit(NULL);
}

// Funcao de teste de comunicação entre componentes de veículos diferentes (externa)
int external_communication_test(std::string networkInterface, int totalMessages) {
    std::cout << "\n============================================================\n"
          << "🚗  TESTE: Comunicação entre componentes de veículos diferentes (externa)\n"
          << "------------------------------------------------------------\n"
          << "Veiculo 0 está buscando os veículos vizinhos ao seu redor:\n"
          << "Componente DetectorVizinhos requisita dados aos componentes GPS dos outros veiculos.\n"
          << "\n============================================================\n"
          << std::endl;

    std::string NETWORK_INTERFACE = networkInterface;
    //const int NUM_MENSAGENS = totalMessages;

    // Cria 10 processos para os veiculos que fornecem sua localizacao.
    for (int i = 0; i < 10; ++i) {
        pid_t pid = fork();

        // Código dos processos dos veiculos que fornecem sua localizacao.
        if (pid == 0) {
        // Cria Veículo.
        Veiculo veiculo(NETWORK_INTERFACE, "Veiculo " + std::to_string(i + 1));
        // Adiciona componentes ao veículo.
        veiculo.criar_componente("GPS " + std::to_string(i + 1), GPS, rotina_gps);
        // Espera as threads componente terminarem.
        veiculo.~Veiculo();
        _exit(0); // Finaliza o processo criado.
        }

    }  // Processo Pai continua o loop para criar o próximo processo.

    // Espera todos os componentes GPS estarem prontos para receber mensagem.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Cria processo para o veiculo com componente DetectorDeVizinhos.
    pid_t pid = fork();
    
    // Código do processo do veiculo detector.
    if (pid == 0) {
        // Cria Veículo.
        Veiculo veiculo(NETWORK_INTERFACE, "Veiculo 0");
        // Adiciona componentes ao veículo.
        veiculo.criar_componente("Detector", DETECTOR_VIZINHOS, rotina_detector_vizinhos);
        // Espera as threads componente terminarem.
        veiculo.~Veiculo();
        _exit(0); // Finaliza o processo criado.
    }
    
    // Processo pai espera todos os filhos
    while (wait(nullptr) > 0); // Espera todos os filhos terminarem

    std::cout << "\n===============================" << std::endl;
    std::cout << "✅ Teste finalizado." << std::endl;
    std::cout << "===============================\n" << std::endl;
    return 0;
}