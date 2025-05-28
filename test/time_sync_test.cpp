#include <iostream>
#include <string>
#include <unistd.h>       // fork(), sleep()
#include <sys/wait.h>     // wait()
#include "../include/vehicle.hpp"

// Parâmetros padrão do teste
int NUM_VEICULOS = 1;          // Quantos veículos aparecem por vez
int NUM_APARICOES = 3;         // Quantas vezes os veículos aparecem
int INTERVALO_APARICAO = 5;   // Tempo entre as aparições (em segundos)
int TEMPO_PERMANENCIA = 15;    // Tempo que cada veículo permanece ativo (em segundos)

int main(int argc, char *argv[]) {
    // Verifica se a interface de rede foi fornecida como argumento
    if (argc < 2) {
        std::cout << "Erro: Por favor, informe a interface de rede.\n";
        std::cout << "Uso: " << argv[0] << " <network-interface> [num_veiculos] [num_aparicoes] [intervalo_aparicao] [tempo_permanencia]\n";
        return 1;
    }

    std::string networkInterface = argv[1];

    // Função auxiliar para ler argumentos opcionais com valor padrão
    auto parse_arg = [&](int index, int default_val) -> int {
        if (argc > index) {
            try {
                return std::stoi(argv[index]);  // Converte argumento para inteiro
            } catch (...) {
                std::cout << "Aviso: parâmetro " << index << " inválido. Usando valor padrão " << default_val << ".\n";
            }
        }
        return default_val;
    };

    // Lê os parâmetros opcionais da linha de comando, se existirem
    NUM_VEICULOS = parse_arg(2, NUM_VEICULOS);
    NUM_APARICOES = parse_arg(3, NUM_APARICOES);
    INTERVALO_APARICAO = parse_arg(4, INTERVALO_APARICAO);
    TEMPO_PERMANENCIA = parse_arg(5, TEMPO_PERMANENCIA);

    // Exibe informações do teste
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

    // Loop principal: para cada aparição
    for (int aparicao = 0; aparicao < NUM_APARICOES; ++aparicao) {
        std::cout << "\n⭐ Aparição #" << (aparicao + 1) << " de " << NUM_APARICOES << std::endl;

        // Cria os processos dos veículos para esta aparição
        for (int i = 0; i < NUM_VEICULOS; ++i) {
            pid_t pid = fork();  // Cria novo processo filho

            if (pid == 0) {  // Processo filho (veículo)
                Veiculo veiculo(networkInterface, "Veiculo" + std::to_string(i + 1));
                // Mantém o veículo "ativo" durante o tempo de permanência
                std::this_thread::sleep_for(std::chrono::seconds(TEMPO_PERMANENCIA));
                return 0;  // Termina o processo filho
            }

            // Pequeno intervalo entre criações de veículos (ajuda a evitar problemas com fork rápido)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Aguarda o tempo entre as aparições
        std::this_thread::sleep_for(std::chrono::seconds(INTERVALO_APARICAO));
    }

    // Espera todos os processos filhos terminarem
    while (wait(NULL) > 0);

    // Fim do teste
    std::cout << "\n===============================" << std::endl;
    std::cout << "✅ Teste finalizado." << std::endl;
    std::cout << "===============================\n" << std::endl;

    return 0;
}
