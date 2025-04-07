#pragma once
#include "ethernet.hpp"
#include "observer.hpp"
#include "message.hpp"
#include "engine.hpp"
#include <memory>
#include <vector>

template <typename Engine>
class NIC : public Ethernet, public Conditionally_Data_Observed<typename Ethernet::Frame, Ethernet::Protocol_Number> {
public:
    typedef Ethernet::Frame Frame;
    typedef Ethernet::Address Address;
    typedef Ethernet::Protocol_Number Protocol_Number;
    
    // Buffer para frames Ethernet
    class Buffer {
    public:
        Frame frame;
        size_t size;
        
        Buffer() : size(0) {}
        Buffer(const Frame& f, size_t s) : frame(f), size(s) {}
    };
    
    // Estatísticas da interface de rede
    struct Statistics {
        size_t tx_packets;
        size_t rx_packets;
        size_t tx_bytes;
        size_t rx_bytes;
        size_t errors;
    };
    
    NIC() : engine(std::make_unique<Engine>()), mac_address({0}) {
        // Inicializa com um MAC aleatório
        for (auto& byte : mac_address) {
            byte = static_cast<uint8_t>(rand() % 256);
        }
        stats = {0};
    }
    
    ~NIC() {
        engine->shutdown();
    }
    
    // Configura o endereço MAC
    void set_address(const Address& addr) {
        mac_address = addr;
    }
    
    // Obtém o endereço MAC
    const Address& get_address() const {
        return mac_address;
    }
    
    // Envia um frame Ethernet
    int send(const Address& dest, Protocol_Number protocol, const void* data, size_t size) {
        if (size > MTU) return -1;
        
        Frame frame;
        frame.dst_mac = dest;
        frame.src_mac = mac_address;
        frame.type = htons(protocol); // Converte para network byte order
        
        memcpy(frame.payload, data, size);
        
        int result = engine->send(&frame, sizeof(Frame::dst_mac) + sizeof(Frame::src_mac) + 
                     sizeof(Frame::type) + size);
        
        if (result > 0) {
            stats.tx_packets++;
            stats.tx_bytes += result;
        } else {
            stats.errors++;
        }
        
        return result;
    }
    
    // Recebe frames Ethernet (chamado pelo Engine quando um frame chega)
    void receive(const Frame* frame, size_t size) {
        if (size < sizeof(Frame::dst_mac) + sizeof(Frame::src_mac) + sizeof(Frame::type)) {
            stats.errors++;
            return;
        }
        
        // Notifica os observadores registrados para este protocolo
        Protocol_Number protocol = ntohs(frame->type); // Converte de network byte order
        Buffer buffer(*frame, size);
        notify(protocol, &buffer);
        
        stats.rx_packets++;
        stats.rx_bytes += size;
    }
    
    // Obtém estatísticas
    const Statistics& get_statistics() const {
        return stats;
    }
    
private:
    std::unique_ptr<Engine> engine;
    Address mac_address;
    Statistics stats;
};