#pragma once
#include "../include/communicator.hpp"
#include "../include/message.hpp"
#include "../include/nic.hpp"
#include "../include/protocol.hpp"
#include "../include/engine.hpp"

#include "../test/agendador.hpp"

#include <string>
#include <pthread.h>
#include <iostream>
#include <unistd.h>

// Classe Veiculo para representar um veículo capaz de criar seus componentes (threads POSIX)
class Veiculo {
    public:
        // Estrutura para passagem de dados para thread componente.
        struct DadosComponente {
            Agendador* agendador;
            Protocol* protocolo;
            const std::string nome;
            Ethernet::Mac_Address id_veiculo;
        };

        // Construtor: inicializa o veiculo, nic e protocolo.
        Veiculo(const std::string& interface, const std::string& nome)
            : nome(nome), nic(interface), protocolo(&nic, 0x88B5) {}

        // Destrutor: espera o término de todas as threads criadas.
        ~Veiculo() {
            for (auto& thread : threads) {
                pthread_join(thread, nullptr);
            }
        }

        // Define o tipo da função que será passada para a thread.
        using funcao = void* (*)(void*);

        // Metodo para criar thread componente.
        bool criar_componente(const std::string nome, funcao func_rotina) {
            pthread_t thread_id;
            // Cria estrutura de dados para passagem de argumentos para a thread.
            DadosComponente* dados = new DadosComponente{&agendador, &protocolo, nome, nic.get_address()};

            // Cria thread POSIX.
            if (pthread_create(&thread_id, nullptr, func_rotina, dados)) {
                delete dados; // Libera a memória alocada
                return false;
            }
            threads.push_back(thread_id);
            return true;
        }
    
    private:
        std::string nome;
        NIC<Engine> nic;
        Protocol protocolo;

        Agendador agendador;

        // Vetor para armazenar os IDs das threads criadas.
        std::vector<pthread_t> threads; // Utilizado para esperar pelo término das threads no destrutor.
};