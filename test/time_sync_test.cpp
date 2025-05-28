#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include "../include/vehicle.hpp"

// Parâmetros do teste
int NUM_VEICULOS = 2; // POR APARICAO
int NUM_APARICOES = 1;
int INTERVALO_APARICAO = 5; // SEGUNDOS
int TEMPO_PERMANENCIA = 30; // SEGUNDOS

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cout << "Erro: Por favor, informe a interface de rede.\n";
        std::cout << "Uso: " << argv[0] << " <network-interface> [num_veiculos] [num_aparicoes] [intervalo_aparicao] [tempo_permanencia]\n";
        return 1;
    }

    std::string networkInterface = argv[1];

    // Função auxiliar para ler argumentos
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
    NUM_APARICOES = parse_arg(3, NUM_APARICOES);
    INTERVALO_APARICAO = parse_arg(4, INTERVALO_APARICAO);
    TEMPO_PERMANENCIA = parse_arg(5, TEMPO_PERMANENCIA);

    std::cout << "============================================================\n"
              << "🧪  TESTE: Sincronização de Tempo entre Veículos\n"
              << "------------------------------------------------------------\n"
              << " Veículos sincronizam seus relógios com o Grandmaster.\n"
              << "============================================================\n"
              << std::endl;

    std::cout << "Parâmetros do teste:\n";
    std::cout << " Interface de rede: " << networkInterface << "\n";
    std::cout << " Número de veículos: " << NUM_VEICULOS << "\n";
    std::cout << " Número de aparições: " << NUM_APARICOES << "\n";
    std::cout << " Intervalo entre aparições: " << INTERVALO_APARICAO << "s\n";
    std::cout << " Tempo de permanência dos veículos: " << TEMPO_PERMANENCIA << "s\n";

    // Cria processos filhos (veículos)
    for (int aparicao = 0; aparicao < NUM_APARICOES; ++aparicao) {
        std::cout << "\n⭐ Aparição #" << (aparicao + 1) << " de " << NUM_APARICOES << std::endl;

        for (int i = 0; i < NUM_VEICULOS; ++i) {
            pid_t pid = fork();
            
            if (pid == 0) { // Processo filho (veículo)
                Veiculo veiculo(networkInterface, "Veiculo" + std::to_string(i + 1));
                // Mantém o veículo ativo pelo tempo especificado
                std::this_thread::sleep_for(std::chrono::seconds(TEMPO_PERMANENCIA));
                return 0;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        std::this_thread::sleep_for(std::chrono::seconds(INTERVALO_APARICAO));
    }

    // Espera todos os veículos desta aparição terminarem
    while (wait(NULL) > 0);

    std::cout << "\n===============================" << std::endl;
    std::cout << "✅ Teste finalizado." << std::endl;
    std::cout << "===============================\n" << std::endl;
    return 0;
}