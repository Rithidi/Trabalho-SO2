#include <../test/internal_communication.cpp>
//#include "../test/external_communication.cpp"

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

    // Chama a função de teste de comunicação interna.
    // Componentes Enviadores 1, 2 e 3 enviam mensagens para o componente Receptor
    internal_communication(networkInterface, totalMessages); 

    // Chama a função de teste de comunicação externa.
    // Componentes Enviadores dos Veiculos B, C e D enviam mensagens para o componente Receptor do Veiculo A
    //external_communication(networkInterface, totalMessages); 

    return 0;
}
