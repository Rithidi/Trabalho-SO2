# 📚 Biblioteca de Comunicação entre Veículos Autônomos

Este projeto implementa uma biblioteca de comunicação segura e confiável para componentes de veículos autônomos. A biblioteca utiliza os conceitos de observação, protocolos personalizados e comunicação assíncrona com suporte a múltiplas threads e processos.


# 📦 Estrutura do Projeto

├── include/        # Arquivos de cabeçalho (Communicator, Message, Protocol, NIC, etc.)
├── src/            # Implementações das classes
├── tests/          # Testes práticos de comunicação interna e externa
├── Makefile        # Script para compilação


# Pré-requisitos:

    1.Makefile: Utilizado para compilar o projeto de forma automatizada.

    2. g++: Compilador do C++.


# 🛠️ Compilação

No diretório raiz do projeto, execute: make

Esse comando irá compilar todos os testes localizados em ./tests/, gerando executáveis com o mesmo nome dos arquivos de teste, por exemplo:

-> internal_communication_test

-> external_communication_test

-> time_sync_test

-> group_communication_test

Para compilar um teste especifico, utilize: make nome_teste


# ⚙️ Executar um teste

Após a compilação, execute o teste desejado com:

sudo ./<nome_teste> <interface_de_rede> [parâmetros_opcionais]


# ✅ Testes Disponíveis

1️⃣ Teste de comunicação em grupos (group_communication_test)
Simula um ambiente dividido em quatro quadrantes (grupos), cada um associado a uma RSU líder local.
Em cada quadrante, são criados veículos estáticos, que mantêm a mesma posição durante toda a simulação:

Um veículo estático é posicionado no centro do quadrante.

Outro é posicionado na borda do quadrante.

Esses veículos solicitam dados de posição a um componente GPS Estático interno.

Além disso, há um veículo dinâmico que percorre o ambiente passando por todos os quadrantes em sentido anti-horário.
Ele parte do centro do primeiro quadrante, visitando o centro e a borda dos demais.

Esse veículo consulta um GPS Dinâmico, que fornece sua posição atualizada ao longo do tempo.
Ele também possui um componente Detector de Veículos, responsável por identificar veículos próximos com base nas informações de localização.

A ideia principal é que o veículo dinâmico, ao se mover, consiga se comunicar com veículos pertencentes ao mesmo grupo ou aos grupos vizinhos.
Dessa forma, o componente de detecção pode continuamente identificar os veículos ao redor que estejam no mesmo grupo ou sejam vizinhos.

    🧵 Componentes
    RSU (classe): Realiza lógica de líder do grupo e de sincronização temporal.

    Veículo (classe): Instancia os componentes.

    GPS Dinâmico (thread): Fornece posições atualizadas dinamicamente durante a simulação.

    GPS Estático (thread): Fornece posição fixa (inalterada) durante toda a simulação.

    Detector de Veículos (thread): Solicita dados de posição aos sensores GPS e identifica veículos próximos.

    🔧 Como Executar

    sudo ./group_communication_test <interface> [intervalo_criacao] [intervalo_posicao] [num_voltas] [periodo_deteccao]

    <interface>: Interface de rede (ex: eth0, wlan0)

    [intervalo_criacao]: (Opcional) Intervalo de tempo (ms) entre a criação dos veículos (padrão: 500)

    [intervalo_posicao]: (Opcional) Intervalo de tempo (ms) em que o veículo dinamico avança sua posição (padrão: 2000)

    [num_voltas]: (Opcional) Número de voltas que o veículo dinâmico realiza durante o teste (padrão: 1)

    [periodo_deteccao]: (Opcional) Periodo de envio das mensagens de interesse do DetectorVeiculos (padrão: 500)

    Exemplo:

    sudo ./time_sync_test eth0 2000 3000 2 500

2️⃣ Teste de sincronização temporal (time_sync_test)
Simula o aparecimento de veiculos ao longo do tempo para acompanhar o funcionamento da classe TimeSyncManager durante o processo de sincronização de tempo.
Imprime etapas realizadas no processo de de sincronização de tempo.
    🔧 Como Executar

    sudo ./time_sync_test <interface> [num_veiculos] [num_aparicoes] [intervalo_aparicao] [tempo_permanencia]

    <interface>: Interface de rede (ex: eth0, wlan0)

    [num_veiculos]: (Opcional) Número de veiculos instanciados por aparição (padrão: 1)

    [num_aparicoes]: (Opcional) Número de aparições (padrão: 3)

    [intervalo_aparicao]: (Opcional) Intervalo de tempo, em segundos, entre novas aparições (padrão: 5)

    [tempo_permanencia]: (Opcional) Período de tempo, em segundos, que os veiculos permanecem ativos (padrão: 15)

    Exemplo:

    sudo ./time_sync_test eth0 1 3 5 15

3️⃣ Comunicação Interna (internal_communication_test)
Valida a comunicação entre componentes dentro do mesmo veículo. Cada Controlador envia requisições periódicas aos Sensores de Temperatura, que respondem com dados simulados.

    🧵 Componentes
    Controlador (thread): Envia interesses e processa mensagens de temperatura.

    Sensor de Temperatura (thread): Responde com dados simulados.

    Veículo (classe): Instancia os componentes.

    🔧 Como Executar

    sudo ./internal_communication_test <interface> [num_controladores] [num_sensores] [num_respostas] [periodo_min] [periodo_max]

    <interface>: Interface de rede (ex: eth0, wlan0)

    [num_controladores]: (Opcional) Quantidade de controladores (padrão: 10)

    [num_sensores]: (Opcional) Quantidade de sensores (padrão: 9)

    [num_respostas]: (Opcional) Respostas esperadas por controlador (padrão: 10)

    [periodo_min]: (Opcional) Período mínimo em ms (padrão: 1)

    [periodo_max]: (Opcional) Período máximo em ms (padrão: 100)

    Exemplo:

    sudo ./internal_communication_test eth0 5 3 10 10 100

4️⃣ Comunicação Externa (external_communication_test)
Simula a interação entre veículos diferentes. Um Detector de Veículos envia requisições periódicas para sensores GPS de outros veículos, que respondem com suas posições.

    🧵 Componentes
    Detector de Veículos (thread): Solicita dados de posição a sensores GPS.

    Sensor GPS (thread): Responde com dados de posição simulados: (x, y).

    Veículo (classe): Instancia os componentes.

    🔧 Como Executar

    sudo ./external_communication_test <interface> [num_veiculos] [num_respostas] [num_aparicoes] [intervalo_aparicao_ms] [intervalo_interesse_ms]

    <interface>: Interface de rede (ex: eth0, lo, tap0)

    [num_veiculos]: (Opcional) Quantidade de veículos por aparição (padrão: 2)

    [num_respostas]: (Opcional) Respostas por sensor (padrão: 3)

    [num_aparicoes]: (Opcional) Total de aparições (padrão: 3)

    [intervalo_aparicao_ms]: (Opcional) Tempo entre aparições (padrão: 1000)

    [intervalo_interesse_ms]: (Opcional) Tempo entre envios de interesse (padrão: 500)

    Exemplo:

    sudo ./external_communication_test tap0 2 3 3 1000 500