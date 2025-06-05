#pragma once

#include "../include/communicator.hpp"
#include "../include/message.hpp"
#include "../include/nic.hpp"
#include "../include/protocol.hpp"
#include "../include/engine.hpp"
#include "../include/data_publisher.hpp"
#include "../include/time_sync_manager.hpp"
#include "../include/rsu_handler.hpp"

#include <string>
#include <pthread.h>
#include <iostream>
#include <unistd.h>
#include <vector>

// Classe que representa um veículo capaz de criar componentes que são executados em threads POSIX
class Veiculo {
public:
    // Estrutura para armazenar os dados que serão passados para cada thread componente
    struct DadosComponente {
        DataPublisher* data_publisher;      // Ponteiro para o DataPublisher do veículo.
        Protocol* protocolo;                // Ponteiro para o protocolo de comunicação do veículo
        const std::string nome;             // Nome do componente/thread
        Ethernet::Mac_Address id_veiculo;   // Endereço MAC do veículo (identificador único)
    };

    // Construtor que inicializa o veículo, a NIC (placa de rede) e o protocolo de comunicação
    Veiculo(const std::string& interface, const std::string& nome);

    // Destrutor que espera o término de todas as threads criadas antes de liberar os recursos
    ~Veiculo();

    // Tipo da função que representa a rotina da thread (função que será executada pela thread)
    using funcao = void* (*)(void*);

    // Cria uma thread que representa um componente do veículo
    // Recebe o nome do componente e a função que a thread vai executar
    bool criar_componente(const std::string nome, funcao func_rotina);

private:
    std::string nome;                   // Nome do veículo
    NIC<Engine> nic;                    // Objeto que representa a interface de rede
    Protocol protocolo;                 // Protocolo de comunicação baseado na NIC

    DataPublisher data_publisher;       // Publicador de dados
    TimeSyncManager time_sync_manager;  // Gerenciador de sincronização de tempo
    RSUHandler rsu_handler;             // Manipulador de RSU (Unidade de Rede Veicular)

    std::vector<pthread_t> threads;       // Vetor que armazena os IDs das threads criadas
};
