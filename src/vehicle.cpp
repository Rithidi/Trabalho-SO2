#include "../include/vehicle.hpp"

// Construtor: inicializa o nome do veículo, a NIC e o protocolo
Veiculo::Veiculo(const std::string& interface, const std::string& nome)
    : nome(nome), nic(interface), protocolo(&nic, &data_publisher, 0x88B5),
     time_sync_manager(&data_publisher, &protocolo, nic.get_address()) {}

// Destrutor: espera todas as threads terminarem antes de destruir o objeto
Veiculo::~Veiculo() {
    for (auto& thread : threads) {
        pthread_join(thread, nullptr); // Aguarda cada thread finalizar
    }
}

// Método para criar uma nova thread componente
// Aloca uma estrutura com os dados necessários para a thread funcionar
// Cria a thread POSIX, passando essa estrutura para a função executada pela thread
// Se falhar na criação da thread, libera a memória alocada e retorna falso
bool Veiculo::criar_componente(const std::string nome, funcao func_rotina) {
    pthread_t thread_id;
    // Aloca os dados que serão passados para a thread
    DadosComponente* dados = new DadosComponente{&data_publisher, &protocolo, nome, nic.get_address()};

    // Cria a thread, executando func_rotina com os dados como argumento
    if (pthread_create(&thread_id, nullptr, func_rotina, dados)) {
        delete dados; // Libera memória se falhar na criação da thread
        return false;
    }
    threads.push_back(thread_id); // Guarda o ID da thread para poder esperar o término depois
    return true;
}
