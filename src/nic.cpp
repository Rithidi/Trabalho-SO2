#include "../include/nic.hpp"
#include "../include/ethernet.hpp"
#include "../include/observer.hpp"

#include <iostream>
#include <unistd.h>


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
      }, true)) { // Member initializer list ends here
    // Inicializa uma semente aleatória.
    srand(getpid()); // Inicializa a semente com o PID do processo atual.
    // Generate a random MAC address
    for (auto& byte : mac_address) {
        byte = static_cast<uint8_t>(rand() % 256);
    }
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
typename NIC<Engine>::Buffer* NIC<Engine>::alloc() {
    Buffer* buffer = new Buffer();  // Cria um novo buffer para o frame Ethernet
    buffer->size = sizeof(Ethernet::Frame); // Define o tamanho do buffer para 1500 bytes.
    return buffer;  // Retorna o ponteiro para o buffer alocado
}

template <typename Engine>
int NIC<Engine>::send(Buffer* buf, bool internal) {
    // Pega o frame do buffer
    Ethernet::Frame* frame = &buf->frame;

    // Verifica se o tamanho do buffer não excede 1500 bytes (tamanho máximo permitido para Ethernet)
    if (buf->size > 1500) return -1;

    int result;

    // Verifica se o endereço de origem e destino são iguais
    if (internal) {
        // Se o endereço de origem e destino forem iguais, envia pelo internal_engine
        result = internal_engine->send(frame, sizeof(*frame)); 
    } else {
        // Se o endereço de origem e destino forem diferentes, envia pelo engine normal
        result = engine->send(frame, sizeof(*frame));
    }

    free(buf);  // Libera o buffer após o envio

    return result;
}


// Método chamado pelo Engine quando um frame é recebido
template <typename Engine>
void NIC<Engine>::receive(const Frame* frame, size_t size) {

    // Aloca dinamicamente um buffer com o frame recebido
    Buffer* buffer = new Buffer(*frame, size);

    // Pega protocolo correspondente ao frame recebido
    Ethernet::Protocol_Number protocol = ntohs(frame->type);

    // Notifica o observador do protocolo correspondente, passando o ponteiro do buffer
    observed.notify(protocol, buffer);
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

