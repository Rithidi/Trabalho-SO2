#include "../include/communicator.hpp"
#include "../include/message.hpp"
#include "../include/nic.hpp"
#include "../include/protocol.hpp"
#include "../include/engine.hpp"

#include "../include/vehicle.hpp"

#include <string>
#include <pthread.h>
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>

// Define os Tipos
Ethernet::Type TIPO_SENSOR_TEMPERATURA = 666;

// Define os parametros do teste
int NUM_CONTROLADORES = 10;  // Numero de Controladores que seram criados.
int NUM_SENSORES = 9;        // Numero de Sensores criados.
int NUM_RESPOSTAS = 10;      // Número de respostas para encerrar requisicao.
int PERIODO_MIN = 1;         // Periodo min de requisicao.
int PERIODO_MAX = 100;       // Periodo max de requisicao.

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
    // Envia mensagem de interesse.
    comunicador.send(&mensagem);
    std::cout << "📬 " << dados->nome << ": enviou interesse para o sensor temperatura com periodo: " << periodo << std::endl;

    // Contador de respostas recebidas.
    int respostas_recebidas = 0;

    while (respostas_recebidas < NUM_RESPOSTAS) {
        // Espera recebimento de mensagens.
        comunicador.receive(&mensagem);

        // Extrai o endereço de destino da mensagem recebida.
        Ethernet::Address endereco_destino = mensagem.getDstAddress();

        // Verifica se a mensagem recebida eh uma resposta (component_id foi preenchido).
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

    // Dicionario para guardar numero de respostas ja enviadas para cada interessado (para encerrar teste).
    std::unordered_map<Thread_ID, int> requisicoes;

    int num_requisicoes_finalizadas = 0; // Numero de interessados que ja receberam todas suas respostas.

    while (num_requisicoes_finalizadas < NUM_CONTROLADORES) {
        // Simula a produção de dados.
        temperatura = 25 + (std::rand() % 6); // Gera número entre 25 e 30

        // Verifica se recebeu alguma mensagem.
        if (comunicador.hasMessage()) {
            // Carrega mensagem recebida.
            comunicador.receive(&mensagem);
            // Verifica se a mensagem recebida eh de interesse (id componente nao foi preenchido).
            if (pthread_equal(mensagem.getDstAddress().component_id, (pthread_t)0)) {
                // Verifica se mensagem de interesse eh para ele.
                if (mensagem.getType() == TIPO_SENSOR_TEMPERATURA) {
                    std::cout << "📬 " << dados->nome << ": recebeu interesse." << std::endl;
                    // Registra periodo no Agendador.
                    dados->agendador->registrar_interesse(pthread_self(), mensagem.getSrcAddress(), mensagem.getPeriod());
                    // Inicializa contador de mensagens ja respondidas ao interessado (apenas para teste). 
                    requisicoes[mensagem.getSrcAddress().component_id] = 0;
                }
            }
        }

        // Verifica se algum periodo de resposta ja foi atingido.
        if (dados->agendador->possui_periodos_atingidos(pthread_self())) {
            // Extrai endereco de destino dos periodos ja atingidos.
            std::vector<Ethernet::Address> destinos = dados->agendador->obter_destinos_prontos(pthread_self());

            for (const auto& destino : destinos) {
                // Prepara mensagem de resposta.
                mensagem.setType(TIPO_SENSOR_TEMPERATURA);
                mensagem.setDstAddress(destino);
                mensagem.setData(reinterpret_cast<char*>(&temperatura), sizeof(int));
                // Envia mensagem de resposta.
                comunicador.send(&mensagem);

                // Incrementa numero de respostas enviadas para interessado.
                requisicoes[destino.component_id]++;

                // Verifica se interessado ja recebeu numero max de respostas.
                if (requisicoes[destino.component_id] == NUM_RESPOSTAS) {
                    num_requisicoes_finalizadas++;
                    // Remove do Agendador o periodo de resposta para esse interessado.
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
int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cout << "Erro: Por favor, informe a interface de rede.\n";
        std::cout << "Uso: " << argv[0] << " <network-interface> [num_controladores] [num_sensores] [num_respostas] [periodo_min] [periodo_max]\n";
        return 1;
    }

    std::string networkInterface = argv[1];

    auto parse_arg = [&](int index, int default_val) -> int {
        if (argc > index) {
            try {
                return std::stoi(argv[index]);
            } catch (...) {
                std::cout << "Aviso: parâmetro " << index << " inválido. Usando valor padrão " << default_val << ".\n";
            }
        }
        return default_val;
    };

    NUM_CONTROLADORES = parse_arg(2, NUM_CONTROLADORES);
    NUM_SENSORES = parse_arg(3, NUM_SENSORES);
    NUM_RESPOSTAS = parse_arg(4, NUM_RESPOSTAS);
    PERIODO_MIN = parse_arg(5, PERIODO_MIN);
    PERIODO_MAX = parse_arg(6, PERIODO_MAX);

    std::cout << "\n"
              << "============================================================\n"
              << "🧪  TESTE: Reconhecimento e Comunicação entre componentes do mesmo veículo (interna)\n"
              << "------------------------------------------------------------\n"
              << " Controladores requisitam dados aos Sensores de Temperatura:\n"
              << "============================================================\n"
              << std::endl;

    std::cout << "Parâmetros do teste:\n";
    std::cout << " Interface de rede: " << networkInterface << "\n";
    std::cout << " Número de controladores: " << NUM_CONTROLADORES << "\n";
    std::cout << " Número de sensores: " << NUM_SENSORES << "\n";
    std::cout << " Número de respostas: " << NUM_RESPOSTAS << "\n";
    std::cout << " Período mínimo (ms): " << PERIODO_MIN << "\n";
    std::cout << " Período máximo (ms): " << PERIODO_MAX << "\n\n";

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