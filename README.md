
# Pré-requisitos:

    1. makefile: Para compilar o código. Para instalar o makefile, use o seguinte comando:
    Para Ubuntu/Debian: sudo apt install make

    2. g++: Compilador do C++. Para instalar o g++, use o seguinte comando:
    Para Ubuntu/Debian: sudo apt install g++


# Como usar:

    1. Compilando o programa
    Primeiro, você precisa compilar o programa. Para isso, execute o seguinte comando no diretório raiz do projeto:

        make

    2. Executando o programa
    Depois de compilar o programa, execute-o com o seguinte comando, também no diretório raiz:

        sudo ./main.exe <network_interface> <totalMessages>

    onde: 
    <network_interface>: Nome da interface de rede que você deseja usar (por exemplo, eth0, wlan0, etc.).

    <totalMessages>: O número total de mensagens que cada componente Enviador irá enviar no teste.


# Exemplo de execução:

    sudo ./main.exe eth0 10

    Esse comando executará o programa na interface de rede eth0 e cada componente Enviador enviará 10 mensagens.


# Testes:

    1. Teste de Comunicação Interna:
    Este teste tem como objetivo validar a comunicação entre componentes de um único veículo, onde os Enviadores a1, b1, c1 enviam mensagens para o componente Receptor 1. Enquanto os Enviadores a2, b2, c2 enviam mensagens para o componente Receptor 2, também no veículo A. Todos os componentes (Enviadores e Receptor) são operados por threads POSIX dentro do mesmo processo.

    2. Teste de Comunicação Externa:
    Este teste tem como objetivo validar a comunicação entre componentes de veículos distintos, onde os Enviadores a1, a2, a3 do Veículo A enviam mensagens para o componente Receptor do Veículo B. Equanto os Enviadores b1, b2, b3 do Veículo B enviam mensagens para o componente Receptor do Veículo A. A comunicação é realizada por meio de processos independentes, criados com fork(), e cada componente (Enviador ou Receptor) é operado por uma thread POSIX.