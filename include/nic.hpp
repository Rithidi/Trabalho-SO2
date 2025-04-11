#pragma once
#include "ethernet.hpp"
#include "observer.hpp"
#include "message.hpp"
#include "engine.hpp"
#include <memory>
#include <vector>
#include <arpa/inet.h>

// NIC (Network Interface Card) - Classe que representa uma interface de rede
// Template Engine: permite usar diferentes implementações de mecanismos de rede
template <typename Engine>
class NIC : public Ethernet {
public:
    typedef Ethernet::Frame Frame;               // Tipo para frames Ethernet
    typedef Ethernet::Address Address;           // Tipo para endereços MAC
    typedef Ethernet::Protocol_Number Protocol_Number;  // Tipo para números de protocolo
    
    // Buffer para armazenar frames Ethernet recebidos
    class Buffer {
    public:
        Frame frame;  // O frame Ethernet
        size_t size;  // Tamanho do payload
        
        Buffer() : size(0) {}  // Construtor padrão
        Buffer(const Frame& f, size_t s) : frame(f), size(s) {}  // Construtor com parâmetros
    };
    
    // Estrutura para armazenar estatísticas da interface de rede
    struct Statistics {
        size_t tx_packets;  // Pacotes transmitidos
        size_t rx_packets;  // Pacotes recebidos
        size_t tx_bytes;   // Bytes transmitidos
        size_t rx_bytes;   // Bytes recebidos
        size_t errors;     // Erros ocorridos
    };
    
    // Construtor: inicializa a NIC com um MAC aleatório e configura a Engine
    NIC(const std::string& interface)
        : engine(std::make_unique<Engine>(interface, [this](const void* data, size_t size) {
            this->receive(reinterpret_cast<const Frame*>(data), size);
        }, true)), mac_address({0}) {
        // Gera um endereço MAC aleatório
        for (auto& byte : mac_address) {
            byte = static_cast<uint8_t>(rand() % 256);
        }
        stats = {0};  // Zera todas as estatísticas
    }
    
    // Destrutor: encerra o engine de rede
    ~NIC() = default; // O std::unique_ptr cuida da destruição da Engine
    
    // Configura o endereço MAC da interface
    void set_address(const Address& addr) {
        mac_address = addr;
    }
    
    // Retorna o endereço MAC atual da interface
    const Address& get_address() const {
        return mac_address;
    }
    

    // Aloca um buffer para armazenar um frame Ethernet
    Buffer* alloc(Address dst, Protocol_Number prot, unsigned int size) {
        if (size > 1500) {
            return nullptr;  // Retorna nullptr se o tamanho exceder o MTU
        }
    
        // Cria um novo buffer para o frame Ethernet
        Buffer* buffer = new Buffer();
        buffer->size = size;                 // Define o tamanho do frame
    
        return buffer;  // Retorna o ponteiro para o buffer alocado
    }

    // Envia um frame Ethernet para o destino especificado
    int send(Buffer* buf) {
        if (buf->size > 1500) return -1;  // Verifica se o tamanho do frame no buffer excede 1500
        
        // Envia o frame diretamente do buffer
        int result = engine->send(&buf->frame, sizeof(buf->frame));
        
        // Atualiza estatísticas
        if (result > 0) {
            stats.tx_packets++;      // Incrementa contador de pacotes enviados
            stats.tx_bytes += result; // Adiciona bytes enviados
        } else {
            stats.errors++;          // Incrementa contador de erros
        }
        
        return result;
    }
    
    // Método chamado pelo Engine quando um frame é recebido
    void receive(const Frame* frame, size_t size) {
        // Verifica se o frame tem tamanho mínimo válido
        
        
        // Converte o tipo de protocolo de network byte order para host byte order
        Protocol_Number protocol = ntohs(frame->type);
        
        // Cria buffer com o frame recebido e notifica os observadores registrados
        Buffer buffer(*frame, size);
        //notify(protocol, &buffer);
        
        // Atualiza estatísticas de recebimento
        stats.rx_packets++;
        stats.rx_bytes += size;
    }
    
    // Retorna as estatísticas atuais da interface
    const Statistics& get_statistics() const {
        return stats;
    }
    
private:
    std::unique_ptr<Engine> engine;  // Mecanismo de rede específico (depende do template)
    Address mac_address;             // Endereço MAC da interface
    Statistics stats;                // Estatísticas de tráfego
};