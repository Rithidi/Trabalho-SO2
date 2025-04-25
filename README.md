
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

    sudo ./main.exe eth0 100

    Esse comando executará o programa na interface de rede eth0 e cada componente enviará 100 mensagens.


# Testes:

    1. Teste de Comunicação Externa:
    Este teste tem como objetivo validar a comunicação entre componentes de veículos distintos, onde os Enviadores dos veículos B, C e D enviam mensagens para o componente Receptor no veículo A. A comunicação é realizada por meio de processos independentes, criados com fork(), e cada componente (Enviador ou Receptor) opera em uma thread POSIX.

    2. Teste de Comunicação Interna:
    Este teste tem como objetivo validar a comunicação entre componentes de um único veículo, onde os Enviadores do veículo A enviam mensagens para o componente Receptor, também no veículo A. Nesse caso, todos os componentes (Enviadores e Receptor) operam como threads POSIX dentro do mesmo processo.