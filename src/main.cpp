#include "../test/internal_communication_test.cpp"
//#include "../test/external_communication_test.cpp"

#include <iostream>
#include <string>

int main(int argc, char *argv[]) {
    // Definindo valores padrão
    std::string networkInterface ;  // Valor padrão
    int totalMessages;  // Valor padrão

    // Verificando se o número de argumentos passados é válido
    if (argc < 2) {
        std::cout << "Erro: Por favor, forneça os parâmetros corretamente." << std::endl;
        std::cout << "Uso: " << argv[0] << " <network-interface>" << std::endl;
        return 1;
    }

    // Lendo os parâmetros diretamente da linha de comando
    networkInterface = argv[1];  // Primeiro argumento: interface de rede

    // Mostrando os valores recebidos
    std::cout << "Interface de rede: " << networkInterface << std::endl;

    internal_communication_test(networkInterface);
    //external_communication_test(networkInterface);
    
    return 0;
}
