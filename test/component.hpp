#include "../include/communicator.hpp"
#include "../include/message.hpp"
#include "../include/protocol.hpp"

#include <string>
#include <pthread.h>
#include <iostream>
#include <unistd.h>


// Classe para encapsular informacoes do componente.
class Componente {
    public:
        // Construtor: inicializa o componente com o protocolo, nome, endereço e porta.
        Componente(Protocol* protocolo, const std::string& nome, Address endereco)
            : nome(nome), endereco(endereco), comunicador(protocolo, endereco.vehicle_id, endereco.component_id, endereco.port) {}

        // Metodo enviar mensagem.
        bool enviar_mensagem(Message* mensagem, Ethernet::Address destino) {
            return comunicador.send(mensagem, destino);
        }

        // Metodo receber mensagem.
        bool recebe_mensagem(Message* mensagem, Ethernet::Address* origem) {
            return comunicador.receive(mensagem, origem);
        }

        // Metodo pegar nome.
        std::string pegar_nome() const {
            return nome;
        }

        // Metodo pegar ID do veiculo.
        Ethernet::Mac_Address pegar_id_veiculo() const {
            return endereco.vehicle_id;
        }

        // Metodo pegar ID do componente.
        Ethernet::Thread_ID pegar_id_componente() const {
            return endereco.component_id;
        }
        
        // Metodo pegar porta.
        Ethernet::Port pegar_porta() const {
            return endereco.port;
        }

    private:
        std::string nome;
        Address endereco;
        Communicator comunicador;
};
