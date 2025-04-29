#include "../include/communicator.hpp"
#include "../include/message.hpp"
#include "../include/nic.hpp"
#include "../include/protocol.hpp"
#include "../include/engine.hpp"

#include <string>
#include <pthread.h>
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>


// Classe base Componente para representar um componente genérico.
class Componente {
    public:
        // Construtor: inicializa o componente com o protocolo, nome, endereço e porta.
        Componente(Protocol* protocolo, const std::string& nome, Address endereco)
            : nome(nome), endereco(endereco), protocolo(protocolo), comunicador(protocolo, endereco.vehicle_id, endereco.component_id, endereco.port) {}

        // Metodo para enviar mensagens.
        void enviar_mensagem(Address endereco_destino, int num_mensagens) {
            int contador = 1;
            Message mensagem;
        
            for (int i = 0; i < num_mensagens; i++) {
                // Prepara os dados da mensagem com o timestamp.
                auto timestamp = std::chrono::high_resolution_clock::now();
                long long timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count();
                std::string dados = "mensagem do " + nome + " (num: " + std::to_string(contador) + ") timestamp: " + std::to_string(timestamp_ms);
                mensagem.setData(dados.c_str(), dados.size() + 1);
        
                // Envia a mensagem.
                if (comunicador.send(&mensagem, endereco_destino)) {
                    std::cout << " 📨 " << nome << ": enviou mensagem " << std::to_string(contador) << std::endl << std::flush;
                } else {
                    std::cout << "Erro ao enviar mensagem." << std::endl;
                }
                contador++;
            }
        }

        // Metodo para receber mensagens.
        void recebe_mensagem(int num_mensagens) {
            std::cout << "📬 " << nome << " esperando por mensagens" << std::endl;
            Message mensagem;
        
            double total_latency = 0.0;
        
            for (int i = 0; i < num_mensagens; i++) {
                if (comunicador.receive(&mensagem)) {
                    // Extrai o timestamp da mensagem recebida.
                    std::string msg(reinterpret_cast<const char*>(mensagem.data()));
                    size_t pos = msg.find("timestamp: ");
                    if (pos != std::string::npos) {
                        long long timestamp_ms = std::stoll(msg.substr(pos + 11));
                        auto now = std::chrono::high_resolution_clock::now();
                        long long now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        
                        // Calcula a latência total.
                        double latency = now_ms - timestamp_ms;
                        total_latency += latency;
        
                        // Imprime a mensagem e a latência.
                        std::cout << "📬 " << nome << ": recebeu " << msg
                                  << " com latência total de " << latency << " ms" << std::endl << std::flush;
                    } else {
                        std::cout << "Erro: timestamp não encontrado na mensagem." << std::endl;
                    }
                } else {
                    std::cout << "Erro ao receber mensagem." << std::endl;
                }
            }
        
            // Calcula e exibe a média de latência.
            double avg_latency = total_latency / num_mensagens;
            std::cout << "📊 " << nome << ": média de latência total = " << avg_latency << " ms" << std::endl;
        }

    private:
        std::string nome;
        Protocol* protocolo;
        Address endereco;
        Communicator comunicador;
    };

// Estrutura para armazenar os dados necessários para a thread do enviador e receptor.
struct dados_thread {
    std::string nome;
    Protocol* protocolo;
    Address endereco_proprio;
    Address endereco_destino; // Endereço de destino para o enviador
    int num_mensagens;
};

// Função que será executada pela thread do enviador.
void* rotina_enviador(void* arg) {
    // Converte o argumento recebido para o tipo apropriado.
    dados_thread dados = *(dados_thread*)arg;
    // Cria uma instância de Enviador e chama o método de envio de mensagens.
    Componente enviador(dados.protocolo, dados.nome, dados.endereco_proprio);
    // Envia mensagens para o endereço de destino.
    enviador.enviar_mensagem(dados.endereco_destino, dados.num_mensagens);
    // Libera a memória alocada para os dados da thread.
    dados_thread* typedArg = static_cast<dados_thread*>(arg);
    delete typedArg;
    pthread_exit(NULL);
}

// Função que será executada pela thread do receptor.
void* rotina_receptor(void* arg) {
    // Converte o argumento recebido para o tipo apropriado.
    dados_thread dados = *(dados_thread*)arg;
    // Cria uma instância de Receptor e chama o método de recebimento de mensagens.
    Componente receptor(dados.protocolo, dados.nome, dados.endereco_proprio);
    // Recebe mensagens.
    receptor.recebe_mensagem(dados.num_mensagens);
    // Libera a memória alocada para os dados da thread.
    dados_thread* typedArg = static_cast<dados_thread*>(arg);
    delete typedArg;
    pthread_exit(NULL);
}

// Classe Veiculo para representar um veículo capaz de criar threads componentes.
class Veiculo {
    public:
        Veiculo(const std::string& interface, const std::string& nome, const Ethernet::Mac_Address& mac)
            : nome(nome), mac_address(mac), nic(interface), protocolo(&nic, 0x88B5) {
            // Registra um dos endereços MAC conhecidos na NIC.
            nic.set_address(mac);
            }

        // Metodo para criar uma thread componente enviador.
        void cria_thread_enviador(const std::string& nome, const Ethernet::Mac_Address& mac, Ethernet::Thread_ID& thread_id,Ethernet::Port porta, 
            Ethernet::Address destino, int num_mensagens) {
            // Cria e preenche estrutura de dados da thread enviador.
            dados_thread* dados = new dados_thread{nome, &protocolo, {mac, thread_id, porta}, destino, num_mensagens};
            // Cria thread POSIX para o novo Enviador.
            pthread_create(&thread_id, nullptr, rotina_enviador, dados);
        }

        // Metodo para criar uma thread componente receptor.
        void cria_thread_receptor(const std::string& nome, const Ethernet::Mac_Address& mac, Ethernet::Thread_ID& thread_id,
             Ethernet::Port porta, int num_mensagens) {
            // Cria e preenche estrutura de dados da thread receptor.
            dados_thread* dados = new dados_thread{nome, &protocolo, {mac, thread_id, porta}, {}, num_mensagens};
            // Cria thread POSIX para o novo Enviador.
            pthread_create(&thread_id, nullptr, rotina_receptor, dados);
        }
        
    private:
        std::string nome;
        Ethernet::Mac_Address mac_address;
        NIC<Engine> nic;
        Protocol protocolo;
};

int teste_comunicacao_interna(std::string networkInterface, int totalMessages) {
    std::cout << "\n"
          << "============================================================\n"
          << "🧪  TESTE: Comunicação entre componentes do mesmo veículo (interna)\n"
          << "------------------------------------------------------------\n"
          << "📨  Enviadores: a1, b1, c1  → 📬 Receptor 1\n"
          << "📨  Enviadores: a2, b2, c2  → 📬 Receptor 2\n"
          << "============================================================\n"
          << std::endl;

    
    std::string NETWORK_INTERFACE = networkInterface;
    const int NUM_MENSAGENS = totalMessages;

    // MACs fictícios
    const Ethernet::Mac_Address MAC_VEICULO_A = {0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0x01};

    // Identificadores de threads
    pthread_t receptor_1_id, receptor_2_id;
    pthread_t a1_id, b1_id, c1_id;
    pthread_t a2_id, b2_id, c2_id;

    // Portas fictícias
    const Ethernet::Port PORTA_RECEPTOR_1 = 5001;
    const Ethernet::Port PORTA_RECEPTOR_2 = 5002;
    const Ethernet::Port PORTA_ENVIADOR = 5000;

    // Endereços para os Receptores
    Ethernet::Address endereco_receptor_1 = {MAC_VEICULO_A, receptor_1_id, PORTA_RECEPTOR_1};
    Ethernet::Address endereco_receptor_2 = {MAC_VEICULO_A, receptor_2_id, PORTA_RECEPTOR_2};

    // Cria Veículo A e Receptores
    Veiculo veiculo_a(NETWORK_INTERFACE, "Veiculo A", MAC_VEICULO_A);
    veiculo_a.cria_thread_receptor("Receptor 1", MAC_VEICULO_A, receptor_1_id, PORTA_RECEPTOR_1, NUM_MENSAGENS * 3);
    veiculo_a.cria_thread_receptor("Receptor 2", MAC_VEICULO_A, receptor_2_id, PORTA_RECEPTOR_2, NUM_MENSAGENS * 3);

    // Cria Enviadores internos.
    veiculo_a.cria_thread_enviador("Enviador a1", MAC_VEICULO_A, a1_id, PORTA_ENVIADOR, endereco_receptor_1, NUM_MENSAGENS);
    veiculo_a.cria_thread_enviador("Enviador a2", MAC_VEICULO_A, a2_id, PORTA_ENVIADOR, endereco_receptor_2, NUM_MENSAGENS);
    veiculo_a.cria_thread_enviador("Enviador b1", MAC_VEICULO_A, b1_id, PORTA_ENVIADOR, endereco_receptor_1, NUM_MENSAGENS);
    veiculo_a.cria_thread_enviador("Enviador b2", MAC_VEICULO_A, b2_id, PORTA_ENVIADOR, endereco_receptor_2, NUM_MENSAGENS);
    veiculo_a.cria_thread_enviador("Enviador c1", MAC_VEICULO_A, c1_id, PORTA_ENVIADOR, endereco_receptor_1, NUM_MENSAGENS);
    veiculo_a.cria_thread_enviador("Enviador c2", MAC_VEICULO_A, c2_id, PORTA_ENVIADOR, endereco_receptor_2, NUM_MENSAGENS);

    // Espera os threads terminarem
    pthread_join(receptor_1_id, nullptr);
    pthread_join(receptor_2_id, nullptr);
    pthread_join(a1_id, nullptr);
    pthread_join(b1_id, nullptr);
    pthread_join(c1_id, nullptr);
    pthread_join(a2_id, nullptr);
    pthread_join(b2_id, nullptr);
    pthread_join(c2_id, nullptr);

    std::cout << "\n===============================" << std::endl;
    std::cout << "✅ Teste finalizado." << std::endl;
    std::cout << "===============================\n" << std::endl;
    return 0;
}

int teste_comunicacao_externa(std::string networkInterface, int totalMessages) {
    std::cout << "\n============================================================\n"
          << "🚗  TESTE: Comunicação entre componentes de veículos diferentes (externa)\n"
          << "------------------------------------------------------------\n"
          << "📨  Enviadores do Veículo A: a1, a2, a3  → 📬 Receptor do Veículo B\n"
          << "📨  Enviadores do Veículo B: b1, b2, b3  → 📬 Receptor do Veículo A\n"
          << "\n============================================================\n"
          << std::endl;

    std::string NETWORK_INTERFACE = networkInterface;
    const int NUM_MENSAGENS = totalMessages;

    // MACs fictícios
    const Ethernet::Mac_Address MAC_VEICULO_A = {0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0x01};
    const Ethernet::Mac_Address MAC_VEICULO_B = {0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0x02};

    // Identificadores de threads
    pthread_t receptor_a_id, receptor_b_id;
    pthread_t a1_id, a2_id, a3_id;
    pthread_t b1_id, b2_id, b3_id;

    // Portas fictícias
    const Ethernet::Port PORTA_RECEPTOR_A = 5001;
    const Ethernet::Port PORTA_RECEPTOR_B = 5002;
    const Ethernet::Port PORTA_ENVIADOR = 5000;

    // Endereços para os Receptores
    Ethernet::Address endereco_receptor_a = {MAC_VEICULO_A, receptor_a_id, PORTA_RECEPTOR_A};
    Ethernet::Address endereco_receptor_b = {MAC_VEICULO_B, receptor_b_id, PORTA_RECEPTOR_B};

    // Cria processo para Veículo A
    pid_t pid_a = fork();

    if (pid_a == 0) {
        // Cria Veículo A e Receptor
        Veiculo veiculo_a(NETWORK_INTERFACE, "Veiculo A", MAC_VEICULO_A);
        veiculo_a.cria_thread_receptor("Receptor A", MAC_VEICULO_A, receptor_a_id, PORTA_RECEPTOR_A, NUM_MENSAGENS * 3);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Cria Enviadores internos (veículo A)
        veiculo_a.cria_thread_enviador("Enviador a1", MAC_VEICULO_A, a1_id, PORTA_ENVIADOR, endereco_receptor_b, NUM_MENSAGENS);
        veiculo_a.cria_thread_enviador("Enviador a2", MAC_VEICULO_A, a2_id, PORTA_ENVIADOR, endereco_receptor_b, NUM_MENSAGENS);
        veiculo_a.cria_thread_enviador("Enviador a3", MAC_VEICULO_A, a3_id, PORTA_ENVIADOR, endereco_receptor_b, NUM_MENSAGENS);
        pthread_join(receptor_a_id, nullptr);
        pthread_join(a1_id, nullptr);
        pthread_join(a2_id, nullptr);
        pthread_join(a3_id, nullptr);
        return 0;
    }

    // Cria processo para Veículo B
    pid_t pid_b = fork();

    if (pid_b == 0) {
        // Cria Veículo B e Receptor
        Veiculo veiculo_b(NETWORK_INTERFACE, "Veiculo B", MAC_VEICULO_B);
        veiculo_b.cria_thread_receptor("Receptor B", MAC_VEICULO_B, receptor_b_id, PORTA_RECEPTOR_B, NUM_MENSAGENS * 3);

        // Cria Enviadores internos (veículo B)
        veiculo_b.cria_thread_enviador("Enviador b1", MAC_VEICULO_B, b1_id, PORTA_ENVIADOR, endereco_receptor_a, NUM_MENSAGENS);
        veiculo_b.cria_thread_enviador("Enviador b2", MAC_VEICULO_B, b2_id, PORTA_ENVIADOR, endereco_receptor_a, NUM_MENSAGENS);
        veiculo_b.cria_thread_enviador("Enviador b3", MAC_VEICULO_B, b3_id, PORTA_ENVIADOR, endereco_receptor_a, NUM_MENSAGENS);
        pthread_join(receptor_b_id, nullptr);
        pthread_join(b1_id, nullptr);
        pthread_join(b2_id, nullptr);
        pthread_join(b3_id, nullptr);
        return 0;
    }
    
    // Processo pai espera todos os filhos
    while (wait(nullptr) > 0); // Espera todos os filhos terminarem

    std::cout << "\n===============================" << std::endl;
    std::cout << "✅ Teste finalizado." << std::endl;
    std::cout << "===============================\n" << std::endl;
    return 0;
}