#include "../include/communicator.hpp"
#include "../include/message.hpp"
#include "../include/nic.hpp"
#include "../include/protocol.hpp"
#include "../include/engine.hpp"

#include "../include/vehicle.hpp"
#include <unistd.h>

#include <string>
#include <pthread.h>
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>

// Define os Tipos
Ethernet::Type TIPO_SENSOR_GPS = 888;

// Estrutura de dados produzida pelo Sensor GPS.
struct DadosSensorGPS {
    int numVeiculo;
    float x;
    float y;
};

// Define os parametros do teste
int NUM_VEICULOS = 2;            // Numero de veiculos por aparicao.
int NUM_RESPOSTAS = 3;           // Numero de respostas por Sensor antes de finalizar.
int NUM_APARICOES = 3;           // Numero de aparicoes.
int INTERVALO_APARICAO = 1000;   // Intervalo de tempo (ms) entre as aparicoes. 
int INTERVALO_INTERESSE = 500;   // Intervalo de tempo (ms) entre os envios de interesse do Detector.

// Funcao de rotina executada pela thread: componente Detector Veiculos.
void* rotina_detector_veiculos(void* arg) {
    Veiculo::DadosComponente* dados = (Veiculo::DadosComponente*)arg;
    Communicator comunicador(dados->protocolo, dados->id_veiculo, pthread_self());

    int num_respostas = 0;
    int num_max_respostas = NUM_VEICULOS * NUM_APARICOES * NUM_RESPOSTAS;

    while (num_respostas < num_max_respostas) {
        // Prepara mensagem de interesse para o sensor gps.
        Message mensagem;

        mensagem.setDstAddress({{0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, (pthread_t)0});   // Deixa o endereco de destino vazio.
        mensagem.setType(TIPO_SENSOR_GPS);  // Preenche tipo do dado

        // Preenche periodo de interesse.
        mensagem.setPeriod(0); // Periodo = 0 => Ping (uma unica resposta).

        // Envia mensagem de interesse.
        comunicador.send(&mensagem);
        std::cout << "📬 " << dados->nome << ": enviou interesse." << std::endl;

        // Espera intervalo de envio de interesses.
        usleep(INTERVALO_INTERESSE * 1000);

        // Processa mensagens recebidas.
        while(comunicador.hasMessage()) {
            Message mensagem;
            comunicador.receive(&mensagem);
            // Verifica se a mensagem recebida é resposta (id componente eh o do componente)
            if (pthread_equal(mensagem.getDstAddress().component_id, pthread_self())) {
                // Verifica tipo de resposta recebida.
                if (mensagem.getType() == TIPO_SENSOR_GPS) {
                    num_respostas++; // incrementa numero de respostas.
                    // Extrai dado recebido.
                    DadosSensorGPS posicao = *reinterpret_cast<DadosSensorGPS*>(mensagem.data());
                    std::cout << "📬 " << dados->nome << ": detectou veiculo " << posicao.numVeiculo << " na posicao: (" << posicao.x << ", " << posicao.y << ")" << std::endl;
                }
            }
        }
    }

    // Informa que terminou de receber todas as respostas.
    //std::cout << "📬 " << dados->nome << ": recebeu TODAS SUAS RESPOSTAS." << std::endl;

    delete dados;
    pthread_exit(NULL);
}

// Funcao de rotina executada pela thread: componente Sensor GPS.
void* rotina_sensor_gps(void* arg) {
    Veiculo::DadosComponente* dados = (Veiculo::DadosComponente*)arg;
    Communicator comunicador(dados->protocolo, dados->id_veiculo, pthread_self());

    // Configura tipos de dados que o componente pode fornecer.
    std::vector<Ethernet::Type> tipos;
    tipos.push_back(TIPO_SENSOR_GPS);

    // Se inscreve no DataPublisher para receber mensagens de interesse nos seus tipos de dados.
    dados->data_publisher->subscribe(comunicador.getObserver(), &tipos);

    DadosSensorGPS posicao;
    posicao.numVeiculo = dados->nome.back() - '0';;
    posicao.x = 0;
    posicao.y = 0;

    int num_respostas_enviadas = 0;

    while (num_respostas_enviadas < NUM_RESPOSTAS) {
        // Verifica se recebeu mensagem.
        if (comunicador.hasMessage()) {
            Message mensagem;
            comunicador.receive(&mensagem);
            
            // Verifica se a mensagem eh de interesse (nao preencheu id componente no endereco de destino).
            if (pthread_equal(mensagem.getDstAddress().component_id, (pthread_t)0)) {
                // Responde a mensagem.
                mensagem.setDstAddress(mensagem.getSrcAddress());
                mensagem.setData(reinterpret_cast<DadosSensorGPS*>(&posicao), sizeof(DadosSensorGPS));
                comunicador.send(&mensagem);
                //std::cout << "📬 " << dados->nome << ": Enviou posicao." << std::endl;
                
                num_respostas_enviadas++;
                // Incrementa posicao.
                posicao.x++;
                posicao.y++;
            
            }
        }
    }

    // Informa que finalizou seus envios.
    //std::cout << "📬 " << dados->nome << ": enviou TODAS SUAS RESPOSTAS." << std::endl;

    delete dados;
    pthread_exit(NULL);
}


void* rotina(void* arg) {
    Veiculo::DadosComponente* dados = (Veiculo::DadosComponente*)arg;
    Communicator comunicador(dados->protocolo, dados->id_veiculo, pthread_self());

    sleep(10);
    delete dados;
    pthread_exit(NULL);
}

// Funcao de teste de comunicação externa.
int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cout << "Erro: Por favor, informe a interface de rede.\n";
        std::cout << "Uso: " << argv[0] << " <network-interface> [num_veiculos] [num_respostas] [num_aparicoes] [intervalo_aparicao_ms] [intervalo_interesse_ms]\n";
        return 1;
    }

    std::string networkInterface = argv[1];

    // Função auxiliar para ler int de argv com fallback para valor default
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

    NUM_VEICULOS = parse_arg(2, NUM_VEICULOS);
    NUM_RESPOSTAS = parse_arg(3, NUM_RESPOSTAS);
    NUM_APARICOES = parse_arg(4, NUM_APARICOES);
    INTERVALO_APARICAO = parse_arg(5, INTERVALO_APARICAO);
    INTERVALO_INTERESSE = parse_arg(6, INTERVALO_INTERESSE);

    std::cout << "\n"
              << "============================================================\n"
              << "🧪  TESTE: Reconhecimento e Comunicação entre componentes de diferentes veículos (externa)\n"
              << "------------------------------------------------------------\n"
              << " Veículo 1 detecta veiculos proximos a ele:\n"
              << " Detector de Veiculos requisita dados aos Sensores GPS.\n"
              << "============================================================\n"
              << std::endl;

    // Imprime os parâmetros que serão usados no teste
    std::cout << "Parâmetros do teste:\n";
    std::cout << " Interface de rede: " << networkInterface << "\n";
    std::cout << " Número de veículos: " << NUM_VEICULOS << "\n";
    std::cout << " Número de respostas: " << NUM_RESPOSTAS << "\n";
    std::cout << " Número de aparições: " << NUM_APARICOES << "\n";
    std::cout << " Intervalo entre aparições (ms): " << INTERVALO_APARICAO << "\n";
    std::cout << " Intervalo entre interesses (ms): " << INTERVALO_INTERESSE << "\n\n";
    
    // Cria Processo.
    pid_t pid = fork();

    if (pid == 0) {
        // Cria Veículo 1.
        Veiculo veiculo(networkInterface, "Veiculo 0");
        // Adiciona componente detector ao veiculo 1.
        veiculo.criar_componente("Detector Veiculos", rotina_detector_veiculos);
        return 0;
    }

    int cont_veiculos = 1;

    // Logica de aparicao de novos veiculos.
    for (int i = 0; i < NUM_APARICOES; i++) {
        // Espera periodo de tempo aleatorio para criar novos veiculos.
        std::this_thread::sleep_for(std::chrono::milliseconds(INTERVALO_APARICAO));
        // Cria novos Processos.
        for (int i = 0; i < NUM_VEICULOS; i++) {
            pid_t pid = fork();

            if (pid == 0) {
                // Cria Veículo.
                Veiculo veiculo(networkInterface, "Veiculo " + std::to_string(cont_veiculos));
                // Adiciona componente sensor gps.
                veiculo.criar_componente("Sensor GPS Veiculo " + std::to_string(cont_veiculos), rotina_sensor_gps);
                std::cout << "*Veiculo " << cont_veiculos << " adicionado" << std::endl;
                return 0;
            }
            cont_veiculos++;
        }
    }

    // Espera todos os processos filhos terminarem
    while (wait(NULL) > 0);

    std::cout << "\n===============================" << std::endl;
    std::cout << "✅ Teste finalizado." << std::endl;
    std::cout << "===============================\n" << std::endl;

    return 0;
}