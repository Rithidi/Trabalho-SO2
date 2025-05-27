
#include <iostream>
#include <string>
#include <pthread.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/wait.h>
#include <set>

#include "../include/vehicle.hpp"


int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cout << "Erro: Por favor, informe a interface de rede.\n";
        std::cout << "Uso: " << argv[0] << " <network-interface>\n";
        return 1;
    }

    std::string networkInterface = argv[1];

    std::cout << "============================================================\n"
              << "🧪  TESTE: Sincronização de Tempo entre Veículos\n"
              << "------------------------------------------------------------\n"
              << " Veículos sincronizam seus relógios com o Grandmaster.\n"
              << "============================================================\n"
              << std::endl;

    std::cout << "Parâmetros do teste:\n";
    std::cout << " Interface de rede: " << networkInterface << "\n";


    pid_t pid = fork();

    if (pid == 0) {
        // Cria Veículo.
        Veiculo veiculo(networkInterface, "Veiculo");
        
        return 0;
    }

    // Espera termino dos processos filhos.
    while (wait(NULL) > 0);

}