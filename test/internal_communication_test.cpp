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

// Define os Tipos
Ethernet::Type TIPO_SENSOR_TEMPERATURA = 666;

// Define os parametros do teste
int NUM_CONTROLADORES;  // Numero de Controladores que seram criados.
int NUM_SENSORES;       // Numero de Sensores criados.
int NUM_RESPOSTAS;      // Número de respostas para encerrar requisicao.
int PERIODO_MIN;        // Periodo min de requisicao.
int PERIODO_MAX;        // Periodo max de requisicao.

// Funcao de rotina executada pela thread: componente Controlador.
void* rotina_controlador(void* arg) {
    Veiculo::DadosComponente* dados = (Veiculo::DadosComponente*)arg;
    Communicator comunicador(dados->protocolo, dados->id_veiculo, pthread_self());
    Message mensagem;

    // Prepara mensagem de interesse para o sensor temperatura.
    mensagem.setDstAddress({dados->id_veiculo, (pthread_t)0});  // Preenche endereço de destino
    mensagem.setType(TIPO_SENSOR_TEMPERATURA);                  // Preenche tipo do dado

    // Preenche periodo de interesse.
    int periodo = PERIODO_MIN + (std::rand() % (PERIODO_MAX - PERIODO_MIN + 1));
    mensagem.setPeriod(periodo);
    comunicador.send(&mensagem);

    std::cout << "📬 " << dados->nome << ": enviou interesse para o sensor temperatura com periodo: " << periodo << std::endl;

    // Contador de respostas recebidas.
    int respostas_recebidas = 0;

    while (respostas_recebidas < NUM_RESPOSTAS) {
        // Espera recebimento de mensagens.
        comunicador.receive(&mensagem);

        // Extrai o endereço de destino da mensagem recebida.
        Ethernet::Address endereco_destino = mensagem.getDstAddress();

        // Verifica se a mensagem recebida eh uma resposta (component_id != 0).
        if (!pthread_equal(endereco_destino.component_id, (pthread_t)0)) {
            // Extrai o tipo da mensagem recebida.
            Ethernet::Type tipo = mensagem.getType();
            // Verifica se a resposta eh do sensor temperatura.
            if (tipo == TIPO_SENSOR_TEMPERATURA) {
                // Incrementa contador de respostas recebidas.
                respostas_recebidas++;
                // Extrai dados da mensagem recebida.
                int dado = *(int*)mensagem.data();
                std::cout << "📬 " << dados->nome << ": recebeu temperatura: " << dado << std::endl;
            }
        }
    }

    // Informa que terminou de receber todas as respostas.
    std::cout << "📬 " << dados->nome << ": recebeu TODAS SUAS RESPOSTAS." << std::endl;

    delete dados;
    pthread_exit(NULL);
}

// Funcao de rotina executada pela thread: componente Sensor Temperatura.
void* rotina_sensor_temperatura(void* arg) {
    Veiculo::DadosComponente* dados = (Veiculo::DadosComponente*)arg;
    Communicator comunicador(dados->protocolo, dados->id_veiculo, pthread_self());
    Message mensagem;
    int temperatura;

    // Dicionario para guardar num_respostas ja enviadas para cada interessado.
    std::unordered_map<Thread_ID, int> requisicoes;

    int num_requisicoes_finalizadas = 0; // Numero de interessados que ja receberam todas suas respostas.

    while (num_requisicoes_finalizadas < NUM_CONTROLADORES) {
        // Simula a produção de dados.
        temperatura = 25 + (std::rand() % 6); // Gera número entre 25 e 30

        // Recebe interesse
        if (comunicador.hasMessage()) {
            comunicador.receive(&mensagem);
            if (mensagem.getPeriod() > 0 && mensagem.getType() == TIPO_SENSOR_TEMPERATURA) {
                std::cout << "📬 " << dados->nome << ": recebeu interesse." << std::endl;
                dados->agendador->registrar_interesse(pthread_self(), mensagem.getSrcAddress(), mensagem.getPeriod());
                requisicoes[mensagem.getSrcAddress().component_id] = 0;
            }
        }

        // Verifica se chegou o momento de enviar
        if (dados->agendador->possui_periodos_atingidos(pthread_self())) {
            std::vector<Ethernet::Address> destinos = dados->agendador->obter_destinos_prontos(pthread_self());

            for (const auto& destino : destinos) {

                if (requisicoes[destino.component_id] == NUM_RESPOSTAS) {
                    continue;
                }

                mensagem.setType(TIPO_SENSOR_TEMPERATURA);
                mensagem.setDstAddress(destino);
                mensagem.setData(reinterpret_cast<char*>(&temperatura), sizeof(int));
                comunicador.send(&mensagem);

                requisicoes[destino.component_id]++; // Incrementa numero de respostas enviadas para interessado.
                if (requisicoes[destino.component_id] == NUM_RESPOSTAS) {
                    num_requisicoes_finalizadas++;
                    dados->agendador->remover_interesse(pthread_self(), destino);
                }
            }
        }
    }

    // Informa que finalizou seus envios.
    std::cout << "📬 " << dados->nome << ": enviou TODAS SUAS RESPOSTAS." << std::endl;

    delete dados;
    pthread_exit(NULL);
}

// Funcao de teste de comunicação interna entre componentes do mesmo veículo.
int internal_communication_test(std::string networkInterface) {
    std::cout << "\n"
          << "============================================================\n"
          << "🧪  TESTE: Reconhecimento e Comunicação entre componentes do mesmo veículo (interna)\n"
          << "------------------------------------------------------------\n"
          << " Controladores requesitam dados aos Sensores de Temperatura:\n"
          << "============================================================\n"
          << std::endl;

    // Define o valor dos parametros do teste
    NUM_CONTROLADORES = 10;  // Numero de Controladores que seram criados.
    NUM_SENSORES = 9;       // Numero de Sensores criados.
    NUM_RESPOSTAS = 10;     // Número de respostas para encerrar uma requisicao.
    PERIODO_MIN = 1;        // Periodo min de uma requisicao.
    PERIODO_MAX = 100;      // Periodo max de uma requisicao.

    // Cria Veículo.
    Veiculo veiculo(networkInterface, "Veiculo");

    // Adiciona componentes ao veículo.
    for (int i = 0; i < NUM_SENSORES; i++) {
        veiculo.criar_componente("Sensor Temperatura " + std::to_string(i + 1), rotina_sensor_temperatura);
    }

    // Espera um tempo para os sensores estarem prontos.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    for (int i = 0; i < NUM_CONTROLADORES; i++) {
        veiculo.criar_componente("Controlador " + std::to_string(i + 1), rotina_controlador);
    }

    return 0;
}