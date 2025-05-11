#include "../include/communicator.hpp"
#include "../include/message.hpp"
#include "../include/nic.hpp"
#include "../include/protocol.hpp"
#include "../include/engine.hpp"

#include "../test/vehicle.hpp"
//#include "../test/component.hpp"

#include <string>
#include <pthread.h>
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>

// Definindo identificadores para os tipos de dados.
const unsigned short CONTROLADOR = 0x0001;
const unsigned short SENSOR_TEMPERATURA = 0x0002;
const unsigned short SENSOR_LIDAR = 0x0003;
const unsigned short SENSOR_CAMERA = 0x0004;

// Estrutura de dados do Sensor LIDAR.
struct PontoLidar { // Coordenada Polar.
    float distancia;
    float angulo;
};

// Funcao de rotina executada pela thread: componente Controlador.
void* rotina_controlador(void* arg) {
    // Converte o argumento recebido para o tipo apropriado.
    Componente* self = (Componente*)arg;

    // Intancia de mensagem.
    Message mensagem;
    
    // Envia mensagem de Interesse para o sensor de temperatura.
    int periodo = 11000; // periodo em ms.
    // Preenche mensagem com periodo de envio.
    mensagem.setData(reinterpret_cast<char*>(&periodo), sizeof(int));
    // Envia mensagem.
    self->enviar_mensagem(&mensagem, {self->pegar_id_veiculo(), (pthread_t)0, SENSOR_TEMPERATURA});
    std::cout << "📬 " << self->pegar_nome() << ": enviou interesse para o sensor de temperatura." << std::endl;

    // Envia mensagem de Interesse para o sensor lidar.
    periodo = 1000;
    // Preenche mensagem com periodo de envio.
    mensagem.setData(reinterpret_cast<char*>(&periodo), sizeof(int));
    // Envia mensagem.
    self->enviar_mensagem(&mensagem, {self->pegar_id_veiculo(), (pthread_t)0, SENSOR_LIDAR});
    std::cout << "📬 " << self->pegar_nome() << ": enviou interesse para o sensor lidar." << std::endl;

    // Envia mensagem de Interesse para o sensor de camera.
    periodo = 6000;
    // Preenche mensagem com periodo de envio.
    mensagem.setData(reinterpret_cast<char*>(&periodo), sizeof(int));
    // Envia mensagem.
    self->enviar_mensagem(&mensagem, {self->pegar_id_veiculo(), (pthread_t)0, SENSOR_CAMERA});
    std::cout << "📬 " << self->pegar_nome() << ": enviou interesse para o sensor de camera." << std::endl;

    while (true) {
        Ethernet::Address origem;
        // Espera recebimento de mensagens do sensor de temperatura.
        self->recebe_mensagem(&mensagem, &origem);

        // Identifica o tipo de dado recebido (através da porta de origem).
        if (origem.port == SENSOR_TEMPERATURA) {
            // Extrai dados da mensagem recebida.
            int dado = *(int*)mensagem.data();
            std::cout << "📬 " << self->pegar_nome() << ": recebeu temperatura: " << dado << std::endl;
        } else if (origem.port == SENSOR_LIDAR) {
            // Extrai dados da mensagem recebida.
            PontoLidar dado;
            memcpy(&dado, mensagem.data(), sizeof(PontoLidar));
            std::cout << "📬 " << self->pegar_nome() << ": recebeu lidar: (" << dado.distancia << ", " << dado.angulo << ")" << std::endl;
        } else if (origem.port == SENSOR_CAMERA) {
            // Extrai dados da mensagem recebida.
            int dado = *(int*)mensagem.data();
            std::cout << "📬 " << self->pegar_nome() << ": recebeu camera: foto " << dado << std::endl;
        }
        //break;
    }
    Componente* typedArg = static_cast<Componente*>(arg);
    delete typedArg;
    pthread_exit(NULL);
}

// Funcao de rotina executada pela thread: componente Sensor Temperatura.
void* rotina_sensor_temperatura(void* arg) {
    // Converte o argumento recebido para o tipo apropriado.
    Componente* self = (Componente*)arg;

    // Define temperatura inicial.
    int temperatura = 25;

    // Espera receber mensagem de interesse de algum componente.
    Message mensagem;
    Ethernet::Address endereco_interessado;
    self->recebe_mensagem(&mensagem, &endereco_interessado);

    // Identifica o tipo do interessado (através da porta de origem).
    if (endereco_interessado.port == CONTROLADOR) {
        std::cout << "📬 " << self->pegar_nome() << ": recebeu interesse do controlador." << std::endl;
    }

    // Extrai periodo de envio da mensagem recebida.
    int periodo = *reinterpret_cast<int*>(mensagem.data());
    
    // Envia periodicamente seus dados para o solicitante.
    while (true) {
        // Espera o periodo definido.
        std::this_thread::sleep_for(std::chrono::milliseconds(periodo));
        // Preenche mensagem com dados do sensor de temperatura.
        mensagem.setData(reinterpret_cast<char*>(&temperatura), sizeof(int));
        
        // Envia mensagem com dados do sensor de temperatura.
        self->enviar_mensagem(&mensagem, endereco_interessado);
        //std::cout << "📬 " << self->pegar_nome() << ": enviou temperatura: " << temperatura << std::endl;

        // Incrementa temperatura para o próximo envio.
        temperatura++;
        //break;
    }
    Componente* typedArg = static_cast<Componente*>(arg);
    delete typedArg;
    pthread_exit(NULL);
}

// Funcao de rotina executada pela thread: componente Sensor LIDAR.
void* rotina_sensor_lidar(void* arg) {
    // Converte o argumento recebido para o tipo apropriado.
    Componente* self = (Componente*)arg;

    // Cria estrutura de dados do sensor LIDAR.
    PontoLidar ponto_lidar{0.0f, 0.0f};

    // Espera receber mensagem de interesse de algum componente.
    Message mensagem;
    Ethernet::Address endereco_interessado;
    self->recebe_mensagem(&mensagem, &endereco_interessado);

    // Identifica o tipo do interessado (através da porta de origem).
    if (endereco_interessado.port == CONTROLADOR) {
        std::cout << "📬 " << self->pegar_nome() << ": recebeu interesse do controlador." << std::endl;
    }

    // Extrai periodo de envio da mensagem recebida.
    int periodo = *reinterpret_cast<int*>(mensagem.data());
    
    // Envia periodicamente seus dados para o solicitante.
    while (true) {
        // Espera o periodo definido.
        std::this_thread::sleep_for(std::chrono::milliseconds(periodo));
        // Preenche mensagem com dados do sensor de temperatura.
        mensagem.setData(reinterpret_cast<char*>(&ponto_lidar), sizeof(PontoLidar));
        
        // Envia mensagem com dados do sensor de temperatura.
        self->enviar_mensagem(&mensagem, endereco_interessado);
        //std::cout << "📬 " << self->pegar_nome() << ": enviou lidar: (" << ponto_lidar.distancia << ", " << ponto_lidar.angulo << ")" << std::endl;

        // Incrementa dados para o próximo envio.
        ponto_lidar.distancia += 0.1f;
        ponto_lidar.angulo += 0.1f;
        //break;
    }
    Componente* typedArg = static_cast<Componente*>(arg);
    delete typedArg;
    pthread_exit(NULL);
}

// Funcao de rotina executada pela thread: componente Camera.
void* rotina_sensor_camera(void* arg) {
    // Converte o argumento recebido para o tipo apropriado.
    Componente* self = (Componente*)arg;

    // Contador de fotos.
    int contador = 1;

    // Espera receber mensagem de interesse de algum componente.
    Message mensagem;
    Ethernet::Address endereco_interessado;
    self->recebe_mensagem(&mensagem, &endereco_interessado);

    // Identifica o tipo do interessado (através da porta de origem).
    if (endereco_interessado.port == CONTROLADOR) {
        std::cout << "📬 " << self->pegar_nome() << ": recebeu interesse do controlador." << std::endl;
    }

    // Extrai periodo de envio da mensagem recebida.
    int periodo = *reinterpret_cast<int*>(mensagem.data());
    
    // Envia periodicamente seus dados para o solicitante.
    while (true) {
        // Espera o periodo definido.
        std::this_thread::sleep_for(std::chrono::milliseconds(periodo));
        // Preenche mensagem com dados do sensor de temperatura.
        mensagem.setData(reinterpret_cast<char*>(&contador), sizeof(int));
        
        // Envia mensagem com dados do sensor de temperatura.
        self->enviar_mensagem(&mensagem, endereco_interessado);
        //std::cout << "📬 " << self->pegar_nome() << ": enviou foto: " << contador << std::endl;

        // Incrementa contador de fotos para o próximo envio.
        contador++;
        //break;
    }
    Componente* typedArg = static_cast<Componente*>(arg);
    delete typedArg;
    pthread_exit(NULL);
}


// Funcao de teste de comunicação interna entre componentes do mesmo veículo.
int internal_communication_test(std::string networkInterface, int totalMessages) {
    std::cout << "\n"
          << "============================================================\n"
          << "🧪  TESTE: Reconhecimento e Comunicação entre componentes do mesmo veículo (interna)\n"
          << "------------------------------------------------------------\n"
          << " Controlador requesita dados aos seguintes componentes:\n"
          << " Sensor Temperatura  /  Sensor LIDAR  /  Sensor Camera\n"
          << "============================================================\n"
          << std::endl;

    std::string NETWORK_INTERFACE = networkInterface;
    const int NUM_MENSAGENS = totalMessages;

    // Define endereco MAC ficticio para NIC do veiculo.
    const Ethernet::Mac_Address MAC_VEICULO = {0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0x01};

    // Cria Veículo.
    Veiculo veiculo(NETWORK_INTERFACE, MAC_VEICULO, "Veiculo");

    // Adiciona componentes ao veículo.
    veiculo.criar_componente("Sensor Temperatura", SENSOR_TEMPERATURA, rotina_sensor_temperatura);
    veiculo.criar_componente("Sensor LIDAR", SENSOR_LIDAR, rotina_sensor_lidar);
    veiculo.criar_componente("Sensor Camera", SENSOR_CAMERA, rotina_sensor_camera);
    veiculo.criar_componente("Controlador", CONTROLADOR, rotina_controlador);

    // Espera as threads componente terminarem.
    veiculo.~Veiculo();

    std::cout << "\n===============================" << std::endl;
    std::cout << "✅ Teste finalizado." << std::endl;
    std::cout << "===============================\n" << std::endl;
    return 0;
}