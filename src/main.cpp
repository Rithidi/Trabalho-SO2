#include "../test/communication.cpp"

#include <iostream>
#include <string>


int main(int argc, char *argv[]) {
    // Definindo valores padrão
    std::string networkInterface ;  // Valor padrão
    int totalMessages;  // Valor padrão

    // Verificando se o número de argumentos passados é válido
    if (argc < 3) {
        std::cout << "Erro: Por favor, forneça os parâmetros corretamente." << std::endl;
        std::cout << "Uso: " << argv[0] << " <network-interface> <total-messages>" << std::endl;
        return 1;
    }

    // Lendo os parâmetros diretamente da linha de comando
    networkInterface = argv[1];  // Primeiro argumento: interface de rede
    totalMessages = std::atoi(argv[2]);  // Segundo argumento: número total de mensagens

    // Mostrando os valores recebidos
    std::cout << "Interface de rede: " << networkInterface << std::endl;
    std::cout << "Número total de mensagens: " << totalMessages << std::endl;

    teste_comunicacao_interna(networkInterface, totalMessages);
    teste_comunicacao_externa(networkInterface, totalMessages);

    return 0;
}
