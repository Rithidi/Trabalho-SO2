#pragma once

#include <memory>
#include <vector>
#include <arpa/inet.h>

#include "ethernet.hpp"
#include "observer.hpp"
#include "message.hpp"
#include "engine.hpp"


template <typename Engine>
class NIC : public Ethernet {
public:
    typedef Ethernet::Frame Frame;                    // Tipo para frames Ethernet
    typedef Ethernet::Mac_Address Mac_Address;        // Tipo para endereços MAC
    typedef Ethernet::Protocol_Number Protocol_Number; // Tipo para números de protocolo
    
    // Buffer para armazenar frames Ethernet recebidos
    class Buffer {
    public:
        Frame frame;   // O frame Ethernet
        size_t size;   // Tamanho do payload
        
        Buffer();
        Buffer(const Frame& f, size_t s);
    };
    
    // Estrutura para armazenar estatísticas da interface de rede
    struct Statistics {
        size_t tx_packets;  // Pacotes transmitidos
        size_t rx_packets;  // Pacotes recebidos
        size_t tx_bytes;    // Bytes transmitidos
        size_t rx_bytes;    // Bytes recebidos
        size_t errors;      // Erros ocorridos
    };
    
    NIC(const std::string& interface);
    ~NIC();
    
    void set_address(const Mac_Address& addr);
    const Mac_Address& get_address() const;
    
    Buffer* alloc(Address dst, Protocol_Number prot, unsigned int size);
    int send(Buffer* buf);
    void receive(const Frame* frame, size_t size);
    const Statistics& get_statistics() const;
    
    void free(Buffer* buf);
    
    void attach(Conditional_Data_Observer* obs);
    void detach(Conditional_Data_Observer* obs);
    
private:
    std::unique_ptr<Engine> engine;           // Mecanismo de rede específico (depende do template)
    Mac_Address mac_address;                  // Endereço MAC da interface
    Statistics stats;                         // Estatísticas de tráfego
    Conditional_Data_Observed observed;
};
