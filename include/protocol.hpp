#pragma once
#include <observer.hpp> // Inclui a classe base Observer e Observed
#include <message.hpp> // Inclui a classe Message


/**
 * @class Protocol
 * @brief Implementa um protocolo de comunicação sobre uma interface de rede (NIC)
 * 
 * @tparam NIC Tipo da interface de rede a ser utilizada
 * 
 * Esta classe fornece:
 * - Gerenciamento de pacotes com cabeçalho próprio
 * - Sistema de endereçamento físico (MAC) + lógico (portas)
 * - Comunicação assíncrona via padrão Observer
 * - Controle de MTU e buffers de rede
 */
template <typename NIC>
class Protocol : private typename NIC::Observer
{
public:
    // Número do protocolo Ethernet (deve ser registrado na IANA)
    static const typename NIC::Protocol_Number PROTO = Traits<Protocol>::ETHERNET_PROTOCOL_NUMBER;
    
    // Tipos derivados da NIC
    typedef typename NIC::Buffer Buffer;            // Tipo do buffer de rede
    typedef typename NIC::Address Physical_Address; // Tipo do endereço físico (MAC)
    typedef uint16_t Port;                          // Tipo para portas lógicas (0-65535)
    
    // Tipos para o padrão Observer
    typedef Conditional_Data_Observer<Buffer, Port> Observer;
    typedef Conditionally_Data_Observed<Buffer, Port> Observed;

    /**
     * @class Address
     * @brief Combina endereço físico e porta lógica para identificação única
     */
    class Address {
    public:
        enum Null { BROADCAST };  // Valor especial para broadcast
        
        // Construtores
        Address() : _paddr(), _port(0) {}  // Endereço vazio
        Address(const Null& null) : _paddr(null), _port(0) {}  // Endereço de broadcast
        Address(Physical_Address paddr, Port port) : _paddr(paddr), _port(port) {}  // Endereço completo
        
        // Operador de conversão para bool (verifica se é válido)
        operator bool() const { return (_paddr || _port); }
        
        // Operador de igualdade
        bool operator==(const Address& a) const { return (_paddr == a._paddr) && (_port == a._port); }
        
    private:
        Physical_Address _paddr;  // Endereço físico (MAC)
        Port _port;               // Porta lógica
    };

    /**
     * @class Header
     * @brief Cabeçalho do protocolo (precede os dados no pacote)
     * 
     * Contém informações de controle e endereçamento
     */
    class Header {
    public:
        Physical_Address src_paddr;  // MAC de origem
        Physical_Address dst_paddr;  // MAC de destino
        Port src_port;               // Porta de origem
        Port dst_port;               // Porta de destino
        uint16_t length;             // Tamanho dos dados
        uint16_t checksum;           // Verificação de integridade
    } __attribute__((packed));       // Sem padding entre campos

    // Tamanho Máximo de Transferência (MTU) - cabeçalho + dados
    static const unsigned int MTU = NIC::MTU - sizeof(Header);

    /**
     * @class Packet
     * @brief Estrutura completa do pacote (cabeçalho + dados)
     */
    class Packet : public Header {
    public:
        Packet() {}
        
        // Retorna ponteiro para o cabeçalho
        Header* header() { return this; }
        
        // Retorna os dados convertidos para o tipo T
        template<typename T>
        T* data() { return reinterpret_cast<T*>(&_data); }
        
    private:
        uint8_t _data[MTU];  // Dados do pacote
    } __attribute__((packed)); // Estrutura compactada

protected:
    /**
     * @brief Construtor protegido
     * @param nic Ponteiro para a interface de rede
     * 
     * Registra o protocolo como observador da NIC
     */
    Protocol(NIC* nic) : _nic(nic) { _nic->attach(this, PROTO); }

public:
    /**
     * @brief Destrutor
     * 
     * Remove o protocolo como observador da NIC
     */
    ~Protocol() { _nic->detach(this, PROTO); }

    /**
     * @brief Envia dados para um endereço de destino
     * @param from Endereço de origem
     * @param to Endereço de destino
     * @param data Ponteiro para os dados
     * @param size Tamanho dos dados em bytes
     * @return Número de bytes enviados ou código de erro negativo
     */
    static int send(Address from, Address to, const void* data, unsigned int size) {
        // Verifica se o tamanho excede o MTU
        if(size > MTU) return -1;
        
        // Aloca buffer na NIC
        Buffer* buf = _nic->alloc(to._paddr, PROTO, sizeof(Header) + size);
        if(!buf) return -2;
        
        // Preenche o cabeçalho do pacote
        Packet* packet = reinterpret_cast<Packet*>(buf->data());
        packet->src_paddr = from._paddr;
        packet->dst_paddr = to._paddr;
        packet->src_port = from._port;
        packet->dst_port = to._port;
        packet->length = htons(size);  // Converte para network byte order
        packet->checksum = 0;          // TODO: Implementar cálculo
        
        // Copia os dados para o pacote
        memcpy(packet->data(), data, size);
        
        // Envia o pacote
        return _nic->send(buf);
    }

    /**
     * @brief Processa um pacote recebido
     * @param buf Buffer com o pacote recebido
     * @param from Ponteiro para armazenar endereço de origem (opcional)
     * @param data Ponteiro para armazenar os dados
     * @param size Tamanho do buffer de dados
     * @return Número de bytes recebidos ou código de erro negativo
     */
    static int receive(Buffer* buf, Address* from, void* data, unsigned int size) {
        const Packet* packet = reinterpret_cast<const Packet*>(buf->data());
        unsigned int packet_size = ntohs(packet->length);  // Converte de network byte order
        
        // Verifica se o buffer de destino é suficiente
        if(packet_size > size) return -1;
        
        // Armazena endereço de origem se solicitado
        if(from) {
            *from = Address(packet->src_paddr, packet->src_port);
        }
        
        // Copia os dados para o buffer de destino
        memcpy(data, packet->data(), packet_size);
        
        // Libera o buffer
        _nic->free(buf);
        
        return packet_size;
    }

    /**
     * @brief Registra um observador para uma porta específica
     * @param obs Ponteiro para o observador
     * @param address Endereço (porta) a ser monitorado
     */
    static void attach(Observer* obs, Address address) {
        _observed.attach(obs, address._port);
    }

    /**
     * @brief Remove um observador de uma porta específica
     * @param obs Ponteiro para o observador
     * @param address Endereço (porta) a ser desmonitorado
     */
    static void detach(Observer* obs, Address address) {
        _observed.detach(obs, address._port);
    }

private:
    /**
     * @brief Callback chamado quando a NIC recebe um pacote
     * @param obs Objeto observado (NIC)
     * @param prot Número do protocolo
     * @param buf Buffer com os dados recebidos
     * 
     * Notifica os observadores registrados para a porta de destino
     * ou libera o buffer se não houver observadores
     */
    void update(typename NIC::Observed* obs, NIC::Protocol_Number prot, Buffer* buf) {
        const Packet* packet = reinterpret_cast<const Packet*>(buf->data());
        if(!_observed.notify(packet->dst_port, buf)) {
            _nic->free(buf);  // Nenhum observador - libera o buffer
        }
    }

private:
    NIC* _nic;                     // Ponteiro para a interface de rede
    static Observed _observed;      // Lista de observadores registrados
};

// Definição da variável estática _observed
template <typename NIC>
typename Protocol<NIC>::Observed Protocol<NIC>::_observed;