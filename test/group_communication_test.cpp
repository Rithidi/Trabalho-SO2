#include "../include/rsu.hpp"
#include "../include/vehicle.hpp"

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <iostream>

int main() {
    RSU rsu_1("enp0s1", 1, {0, 100, 0, 100}); // ID do grupo 1, Quadrante de 0 a 100 em X e Y.
    //RSU rsu_2("enp0s1", 2, {-100, 0, 0, 100});
    //RSU rsu_3("enp0s1", 3, {-100, 0, -100, 0});
    //RSU rsu_4("enp0s1", 4, {0, 100, -100, 0});

    // Cria Processo.
    pid_t pid = fork();

    if (pid == 0) {
        // Cria Veículo 1.
        Veiculo veiculo("enp0s1", "Veiculo 0");
        std::this_thread::sleep_for(std::chrono::seconds(20));
        return 0;
    }

    // Espera todos os processos filhos terminarem
    while (wait(NULL) > 0);

    std::this_thread::sleep_for(std::chrono::seconds(30));

    return 0;
}