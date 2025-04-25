#include "../include/communicator.hpp"
#include "../include/message.hpp"
#include "../include/nic.hpp"
#include "../include/protocol.hpp"
#include "../include/engine.hpp"

#include <string>
#include <pthread.h>
#include <iostream>


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


int internal_communication(std::string networkInterface, int totalMessages) {
    std::cout << "\nTESTE: Comunicação entre componentes do mesmo veículo (mesmo processo)" << std::endl;
    std::cout << "  Componentes Enviadores 1, 2 e 3 enviam mensagens para o componente Receptor\n" << std::endl;

    // Define o número de mensagens a serem enviadas.
    const int NUM_MENSAGENS = totalMessages;

    // Endereços MAC fictícios.
    const Ethernet::Mac_Address MAC_VEICULO_A = {0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0x01};

    // Identificadores dos Componentes.
    pthread_t receptor_1_id;
    pthread_t receptor_2_id;
    pthread_t enviador_1_id;
    pthread_t enviador_2_id;
    pthread_t enviador_3_id;
    pthread_t enviador_4_id;

    // Portas para os componentes.
    const Ethernet::Port PORTA_RECEPTOR = 5001;
    const Ethernet::Port PORTA_ENVIADOR = 5000;

    // Cria o veículo A.
    Veiculo veiculo_a(networkInterface, "Veiculo A", MAC_VEICULO_A);

    // Adiciona componente Receptor no veiculo A.
    veiculo_a.adicionar_componente_receptor("Receptor A", MAC_VEICULO_A, receptor_1_id, PORTA_RECEPTOR, NUM_MENSAGENS*3);
    // Adiciona componente Receptor no veiculo A.
    veiculo_a.adicionar_componente_receptor("Receptor B", MAC_VEICULO_A, receptor_2_id, PORTA_RECEPTOR, NUM_MENSAGENS);

    // Monta o endereço de destino para os enviadores.
    Ethernet::Address endereco_receptor_a = {MAC_VEICULO_A, receptor_1_id, PORTA_RECEPTOR};
    Ethernet::Address endereco_receptor_b = {MAC_VEICULO_A, receptor_2_id, PORTA_RECEPTOR};

    // Adiciona os componentes Enviadores no veiculo A.
    veiculo_a.adicionar_componente_enviador("Enviador 1", MAC_VEICULO_A, enviador_1_id, PORTA_ENVIADOR, endereco_receptor_a, NUM_MENSAGENS);
    veiculo_a.adicionar_componente_enviador("Enviador 2", MAC_VEICULO_A, enviador_2_id, PORTA_ENVIADOR, endereco_receptor_a, NUM_MENSAGENS);
    veiculo_a.adicionar_componente_enviador("Enviador 3", MAC_VEICULO_A, enviador_3_id, PORTA_ENVIADOR, endereco_receptor_a, NUM_MENSAGENS);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000)); // Ajuste o delay conforme necessário
    veiculo_a.adicionar_componente_enviador("Enviador 4", MAC_VEICULO_A, enviador_4_id, PORTA_ENVIADOR, endereco_receptor_b, NUM_MENSAGENS);
    return 0;
}