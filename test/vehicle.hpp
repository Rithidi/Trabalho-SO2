#include "../include/communicator.hpp"
#include "../include/message.hpp"
#include "../include/nic.hpp"
#include "../include/protocol.hpp"
#include "../include/engine.hpp"

#include "../test/component.hpp"

#include <string>
#include <pthread.h>
#include <iostream>
#include <unistd.h>

// Classe Veiculo para representar um veículo capaz de criar seus componentes (threads POSIX)
class Veiculo {
    public:
        Veiculo(const std::string& interface, const Ethernet::Mac_Address& mac, const std::string& nome)
            : nome(nome), mac_address(mac), nic(interface), protocolo(&nic, 0x88B5) {
            // Registra um dos endereços MAC conhecidos na NIC.
            nic.set_address(mac);
        }

        // Destrutor: espera o término de todas as threads criadas.
        ~Veiculo() {
            for (auto& thread : threads) {
                pthread_join(thread, nullptr);
            }
        }

        // Define o tipo da função que será passada para a thread
        using funcao = void* (*)(void*);

        // Metodo para criar componentes.
        bool criar_componente(const std::string& nome, Ethernet::Port porta, funcao func_rotina) {
            pthread_t thread_id;
            // Cria e preenche Componente.
            Componente* self = new Componente(&protocolo, nome, {mac_address, thread_id, porta});
            // Cria thread POSIX.
            if (pthread_create(&thread_id, nullptr, func_rotina, self)) {
                delete self; // Libera a memória alocada
                return false;
            }
            threads.push_back(thread_id);
            return true;
        }
        
    private:
        std::string nome;
        Ethernet::Mac_Address mac_address;
        NIC<Engine> nic;
        Protocol protocolo;

        // Vetor para armazenar os IDs das threads criadas.
        std::vector<pthread_t> threads; // Utilizado para esperar o término das threads no destrutor.
};