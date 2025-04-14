Como utilizar o Makefile

Este projeto utiliza um `Makefile` para facilitar a compilação e configuração do programa. 
Siga as instruções abaixo para compilar e executar o programa.

--- Comandos básicos ---

1. Compilar o programa com valores padrão:
   make
   - Compila o programa utilizando:
     - Interface de rede padrão: `lo` (loopback). 
     - Atenção: Pacotes podem ser recebidos duas vezes na interface `lo` devido ao comportamento da rede loopback.
     - Número total de mensagens padrão: `1000`.

2. Compilar o programa com uma interface de rede específica:
   make NETWORK_INTERFACE=sua_rede
   - Substitui a interface de rede padrão (`lo`) pela interface desejada.
   - Recomenda-se utilizar uma interface de rede real (como `eth0` ou `wlan0`) para evitar duplicação de pacotes.

3. Compilar o programa com um número específico de mensagens:
   make TOTAL_MESSAGES=n
   - Substitui o número total de mensagens padrão (`1000`) pelo número desejado.

4. Compilar o programa com interface de rede e número de mensagens personalizados:
   make NETWORK_INTERFACE=wlan0 TOTAL_MESSAGES=200

5. Limpar os arquivos gerados:
   make clean
   - Remove o executável gerado (`main.exe`).

--- Executando o programa ---

Após compilar o programa, você pode executá-lo com o seguinte comando:
sudo ./main.exe

--- Como o teste funciona ---

O teste simula a comunicação entre dois componentes de veículos (processos distintos) utilizando mensagens enviadas e recebidas por meio de uma interface de rede.

1. **Processo Pai**:
   - Envia mensagens simuladas (`TOTAL_MESSAGES`) para o processo filho.
   - Utiliza a interface de rede configurada (`NETWORK_INTERFACE`).

2. **Processo Filho**:
   - Recebe as mensagens enviadas pelo processo pai.
   - Exibe cada mensagem recebida e o total de mensagens processadas.

3. **Execução**:
   - O programa é executado após a compilação com o comando `./main.exe`.
   - O comportamento pode ser configurado pelo Makefile, ajustando o número de mensagens (`TOTAL_MESSAGES`) e a interface de rede (`NETWORK_INTERFACE`).

--- Observações ---

- Certifique-se de que o compilador `g++` está instalado no sistema.
- O programa utiliza a biblioteca `pthread`, que deve estar disponível no sistema.
- Para evitar duplicação de pacotes, evite usar a interface `lo` (loopback) e prefira uma interface de rede real, como `eth0` ou `wlan0`.
- Para personalizar o comportamento do programa, utilize as variáveis `NETWORK_INTERFACE` e `TOTAL_MESSAGES` no comando `make`.