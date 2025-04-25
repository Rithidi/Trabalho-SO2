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
#include <chrono>


// Classe base Componente para representar um componente genérico.
class Componente {
    public:
        Componente(const std::string& nome, Protocol* protocolo, const Ethernet::Mac_Address& mac, Ethernet::Thread_ID thread_id, Ethernet::Port porta)
            : nome(nome), veiculo_id(mac), componente_id(thread_id), porta(porta), protocolo(protocolo), comunicador(protocolo, mac, componente_id, porta) {
            // Cria thread para o componente.
            pthread_create(&componente_id, nullptr, &Componente::rotina_helper, this);
        }
    
        // Método virtual que será sobrescrito pelas classes derivadas
        virtual void* rotina() {
            return nullptr;
        }
    
        // Método estático helper para chamar o método de instância
        static void* rotina_helper(void* context) {
            return static_cast<Componente*>(context)->rotina();
        }
    
        pthread_t get_id() const {
            return componente_id;
        }
    
    protected:
        std::string nome;
        Protocol* protocolo;
        Ethernet::Mac_Address veiculo_id;
        pthread_t componente_id;
        Ethernet::Port porta;
        Communicator comunicador;
    };


// Classe Enviador para representar componente que envia mensagens.
class Enviador : public Componente {
    public:
        Enviador(const std::string& nome, Protocol* protocolo, const Ethernet::Mac_Address& mac, Ethernet::Thread_ID thread_id, Ethernet::Port porta, 
            Ethernet::Address destino, int num_mensagens)
            : endereco_destino(destino), num_mensagens(num_mensagens),Componente(nome, protocolo, mac, thread_id, porta) {
            // Inicializa o sensor.
            }

        void enviar_mensagem() {
            int contador = 1;
            Message mensagem;

            for (int i = 0; i < num_mensagens; i++) {
                // Prepara os dados da mensagem.
                std::string dados = "Mensagem do " + nome + " Num: " + std::to_string(contador);
                mensagem.setData(dados.c_str(), dados.size() + 1);
                // Envia a mensagem.
                if (comunicador.send(&mensagem, endereco_destino)) {
                    std::cout << "Mensagem enviada: " << dados << std::endl;
                } else {
                    std::cout << "Erro ao enviar mensagem." << std::endl;
                }
                contador++;
                // Adiciona pequeno delay entre os envios para evitar sobrecarga.
                std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Ajuste o delay conforme necessário
            }
            // Informa que o enviador terminou de enviar mensagens.
            std::cout << "** " << nome << " enviou todas suas mensagens." << std::endl;
        }

        // Sobrecarrega o método de rotina da thread para enviar mensagens.
        void* rotina() {
            enviar_mensagem();
            return nullptr;
        }

    private:
        // Guarda o endereço de destino das mensagens.
        Ethernet::Address endereco_destino;
        // Guarda o número de mensagens a serem enviadas.
        int num_mensagens;
};


// Classe Receptor para representar componente que recebe mensagens.
class Receptor : public Componente {
    public:
        Receptor(const std::string& nome, Protocol* protocolo, const Ethernet::Mac_Address& mac, Ethernet::Thread_ID thread_id, Ethernet::Port porta, int num_mensagens)
            : num_mensagens(num_mensagens), Componente(nome, protocolo, mac, thread_id, porta) {
            // Inicializa o sensor.
            }

        void recebe_mensagem() {
            Message mensagem;
            for (int i = 0; i < num_mensagens; i++) {
                // Recebe a mensagem.
                if (comunicador.receive(&mensagem)) {
                    // Converte os dados da mensagem para string.
                    std::string msg(reinterpret_cast<const char*>(mensagem.data()));
                    // Imprime a mensagem recebida.
                    std::cout << "Mensagem recebida: " << msg.data() << std::endl;
                } else {
                    std::cout << "Erro ao receber mensagem." << std::endl;
                }
            }
            // Informa que o receptor terminou de receber mensagens.
            std::cout << "** " << nome << " recebeu todas as mensagens." << std::endl;
        }

        // Sobrecarrega o método de rotina da thread para receber mensagens.
        void* rotina() {
            recebe_mensagem();
            return nullptr;
        }
    private:
        // Guarda o número de mensagens a serem recebidas.
        int num_mensagens;
};


// Classe Veiculo para representar um veículo com componentes de comunicação.
class Veiculo {
    public:
        Veiculo(const std::string& interface, const std::string& nome, const Ethernet::Mac_Address& mac)
            : nome(nome), mac_address(mac), nic(interface), protocolo(&nic, 0x88B5) {
            // Registra um dos endereços MAC conhecidos na NIC.
            nic.set_address(mac);
            }

        ~Veiculo() {
            // Espera até que todas as threads Componente terminem.
            for (auto& componente : componentes) {
                pthread_join(componente->get_id(), nullptr);
                delete componente; // Libera a memória alocada para o componente.
            }
        }

        // Metodo adicionar componentes.
        void adicionar_componente_enviador(const std::string& nome, const Ethernet::Mac_Address& mac, Ethernet::Thread_ID thread_id,Ethernet::Port porta, 
            Ethernet::Address destino, int num_mensagens) {
            Enviador* novo_enviador = new Enviador(nome, &protocolo, mac, thread_id, porta, destino, num_mensagens);
            // Adicionar à lista de componentes.
            componentes.push_back(novo_enviador);
        }

        void adicionar_componente_receptor(const std::string& nome, const Ethernet::Mac_Address& mac, Ethernet::Thread_ID thread_id,
             Ethernet::Port porta, int num_mensagens) {
            Receptor* novo_receptor = new Receptor(nome, &protocolo, mac, thread_id, porta, num_mensagens);
            // Adicionar à lista de componentes.
            componentes.push_back(novo_receptor);
        }
        
    private:
        std::string nome;
        Ethernet::Mac_Address mac_address;
        NIC<Engine> nic;
        Protocol protocolo;
        // Adicionar lista de componentes.
        std::vector<Componente*> componentes;
};


int external_communication(std::string interface, int totalMessages) {
    std::cout << "\nTESTE: Comunicação entre componentes de veículos diferentes (processos diferentes)" << std::endl;
    std::cout << "  Componentes Enviadores dos Veiculos B, C e D enviam mensagens para o componente Receptor do Veiculo A\n" << std::endl;

    // Variáveis globais.
    std::string network_interface = interface;
    int num_mensagens = totalMessages;

    // Endereço MAC fictício para o veiculo receptor.
    const Ethernet::Mac_Address MAC_VEICULO_A = {0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0x01};
    // Endereços MAC fictícios para os veículos enviando mensagens.
    const Ethernet::Mac_Address MAC_VEICULO_B = {0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0x02};
    const Ethernet::Mac_Address MAC_VEICULO_C = {0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0x03};
    const Ethernet::Mac_Address MAC_VEICULO_D = {0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0x04};

    // Identificadores dos Componentes.
    pthread_t receptor_id;
    pthread_t enviador_b_id;
    pthread_t enviador_c_id;
    pthread_t enviador_d_id;

    // Portas para os componentes.
    const Ethernet::Port PORTA_RECEPTOR = 5001;
    const Ethernet::Port PORTA_ENVIADOR = 5000;

    // Monta o endereço de destino (Componente Receptor do Veiculo A) dos enviadores.
    Ethernet::Address endereco_receptor_a = {MAC_VEICULO_A, receptor_id, PORTA_RECEPTOR};

    pid_t pid_recebedor = fork();

    if (pid_recebedor == -1) {
        std::cout << "Erro ao criar o processo do receptor" << std::endl;
        return 1;
    }

    if (pid_recebedor == 0) {
        // Processo filho: receptor (Veículo A)
        // Cria o veículo A.
        Veiculo veiculo_a(interface, "Veiculo A", MAC_VEICULO_A);
        // Adiciona componente Receptor no veiculo A.
        veiculo_a.adicionar_componente_receptor("Receptor A", MAC_VEICULO_A, receptor_id, PORTA_RECEPTOR, num_mensagens*3);
        return 0;
    }

    // Pequeno delay para garantir que o Receptor esteja pronto antes de iniciar os Enviadores.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Criar processo para Veículo B
    pid_t pid_b = fork();

    if (pid_b == 0) {
        // Cria o veículo B.
        Veiculo veiculo_b(interface, "Veiculo B", MAC_VEICULO_B);
        // Adiciona componente Enviador no veiculo B.
        veiculo_b.adicionar_componente_enviador("Enviador B", MAC_VEICULO_B, enviador_b_id, PORTA_ENVIADOR, endereco_receptor_a, num_mensagens);
        return 0;
    }

    // Criar processo para Veículo C
    pid_t pid_c = fork();

    if (pid_c == 0) {
        // Cria o veículo C.
        Veiculo veiculo_b(interface, "Veiculo C", MAC_VEICULO_C);
        // Adiciona componente Enviador no veiculo B.
        veiculo_b.adicionar_componente_enviador("Enviador C", MAC_VEICULO_C, enviador_c_id, PORTA_ENVIADOR, endereco_receptor_a, num_mensagens);
        return 0;
    }

    // Criar processo para Veículo D
    pid_t pid_d = fork();

    if (pid_d == 0) {
        // Cria o veículo D.
        Veiculo veiculo_b(interface, "Veiculo D", MAC_VEICULO_D);
        // Adiciona componente Enviador no veiculo B.
        veiculo_b.adicionar_componente_enviador("Enviador D", MAC_VEICULO_D, enviador_d_id, PORTA_ENVIADOR, endereco_receptor_a, num_mensagens);
        return 0;
    }

    // Processo pai espera todos os filhos
    while (wait(nullptr) > 0); // Espera todos os filhos terminarem

    return 0;
}