#include "../include/nic.hpp"
#include "../include/ethernet.hpp"
#include "../include/observer.hpp"

#include <iostream>


typedef Ethernet::Mac_Address Mac_Address;

// Definições para o Buffer
template <typename Engine>
NIC<Engine>::Buffer::Buffer() : size(0) {}

template <typename Engine>
NIC<Engine>::Buffer::Buffer(const Frame& f, size_t s) : frame(f), size(s) {}

// Construtor da classe NIC
template <typename Engine>
NIC<Engine>::NIC(const std::string& interface)
    : engine(std::make_unique<Engine>(interface, [this](const void* data, size_t size) {
          this->receive(reinterpret_cast<const Frame*>(data), size);
      }, true)),
      internal_engine(std::make_unique<InternalEngine>(interface, [this](const void* data, size_t size) {
          this->receive(reinterpret_cast<const Frame*>(data), size);
      }, true)),
      mac_address({0}) { // Member initializer list ends here
    // Generate a random MAC address
    for (auto& byte : mac_address) {
        byte = static_cast<uint8_t>(rand() % 256);
    }
    stats = {0}; // Reset all statistics
}

// Destruidor da classe NIC
template <typename Engine>
NIC<Engine>::~NIC() = default;  // O std::unique_ptr cuida da destruição da Engine

// Configura o endereço MAC da interface
template <typename Engine>
void NIC<Engine>::set_address(const Mac_Address& addr) {
    mac_address = addr;
}

// Retorna o endereço MAC atual da interface
template <typename Engine>
const Mac_Address& NIC<Engine>::get_address() const {
    return mac_address;
}

// Aloca um buffer para armazenar um frame Ethernet
template <typename Engine>
typename NIC<Engine>::Buffer* NIC<Engine>::alloc(Address dst, Protocol_Number prot, unsigned int size) {
    if (size > 1500) {
        return nullptr;  // Retorna nullptr se o tamanho exceder o MTU
    }

    // Cria um novo buffer para o frame Ethernet
    Buffer* buffer = new Buffer();
    buffer->size = size;  // Define o tamanho do frame

    return buffer;  // Retorna o ponteiro para o buffer alocado
}

template <typename Engine>
int NIC<Engine>::send(Buffer* buf) {
    Ethernet::Frame* frame = &buf->frame;

    // Corrige o tamanho real da estrutura Frame
    //std::cout << "NIC enviando frame para engine. protocolo: " << frame->type << " tamanho frame: " << sizeof(*frame) << std::endl;

    // Verifica se o tamanho do buffer não excede 1500 bytes (tamanho máximo permitido para Ethernet)
    if (buf->size > 1500) return -1;

    // Envia o frame diretamente do buffer para o engine
    int result = engine->send(frame, sizeof(*frame));  // Usa sizeof(*frame) para obter o tamanho real da estrutura

    free(buf);  // Libera o buffer após o envio

    return result;
}


// Método chamado pelo Engine quando um frame é recebido
template <typename Engine>
void NIC<Engine>::receive(const Frame* frame, size_t size) {

    //std::cout << "NIC recebeu frame da engine. protocolo: " << ntohs(frame->type) << " tamanho frame: " << sizeof(*frame) << std::endl;

    // Cria buffer com o frame recebido e notifica os observadores registrados
    Buffer buffer(*frame, size);

    // Pega protocolo correspondente ao frame recebido
    Ethernet::Protocol_Number protocol = ntohs(frame->type);

    // Notifica o observador do protocolo correspondente
    observed.notify(protocol, &buffer);
}

// Retorna as estatísticas atuais da interface
template <typename Engine>
const typename NIC<Engine>::Statistics& NIC<Engine>::get_statistics() const {
    return stats;
}

// Libera o buffer
template <typename Engine>
void NIC<Engine>::free(Buffer* buf) {
    delete buf;  // Libera a memória alocada para o buffer
}

// Adiciona um observador
template <typename Engine>
void NIC<Engine>::attach(Conditional_Data_Observer* obs) {
    observed.attach(obs);
}

// Remove um observador
template <typename Engine>
void NIC<Engine>::detach(Conditional_Data_Observer* obs) {
    observed.detach(obs);
}

// Instancia o template para os tipos necessários (se você precisar de uma versão específica)
template class NIC<Engine>;  // Aqui você precisa especificar qual engine usar

