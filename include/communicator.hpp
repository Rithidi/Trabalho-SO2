#pragma once

#include "observer.hpp" // Inclui a classe base Observer e Observed
#include "message.hpp"  // Inclui a classe Message

/**
 * @class Communicator
 * @brief Classe template para comunicação entre componentes usando um canal específico.
 * 
 * Esta classe implementa um padrão Observer para receber mensagens de forma assíncrona
 * e fornece métodos para envio de mensagens através de um canal de comunicação.
 * 
 * @tparam Channel Tipo do canal de comunicação a ser utilizado.
 */
template <typename Channel>
class Communicator : public Concurrent_Observer<
    typename Channel::Observer::Observed_Data,
    typename Channel::Observer::Observing_Condition>
{
    // Define um alias para a classe base Observer para simplificar o código
    typedef Concurrent_Observer<
        typename Channel::Observer::Observed_Data,
        typename Channel::Observer::Observing_Condition> Observer;

public:
    // Tipos públicos derivados do Channel
    typedef typename Channel::Buffer Buffer;   // Tipo do buffer de comunicação
    typedef typename Channel::Address Address; // Tipo do endereço no canal

    /**
     * @brief Construtor da classe Communicator
     * @param channel Ponteiro para o canal de comunicação
     * @param address Endereço deste comunicador no canal
     * 
     * Registra este comunicador no canal fornecido com o endereço especificado.
     */
    Communicator(Channel* channel, Address address)
        : _channel(channel), _address(address)
    {
        _channel->attach(this, _address);
    }

    /**
     * @brief Destrutor da classe
     * 
     * Remove o registro deste comunicador do canal.
     */
    ~Communicator() {
        _channel->detach(this, _address);
    }

    /**
     * @brief Envia uma mensagem através do canal
     * @param message Ponteiro para a mensagem a ser enviada
     * @return true se a mensagem foi enviada com sucesso, false caso contrário
     */
    bool send(const Message* message) {
        return (_channel->send(_address,
                               Channel::Address::BROADCAST,
                               message->data(),
                               message->size()) > 0);
    }

    /**
     * @brief Recebe uma mensagem do canal
     * @param message Ponteiro onde a mensagem recebida será armazenada
     * @return true se uma mensagem foi recebida com sucesso, false caso contrário
     * 
     * Este método bloqueia a execução até que uma mensagem esteja disponível.
     */
    bool receive(Message* message) {
        Buffer* buf = Observer::updated(); // Bloqueia até notificação
        typename Channel::Address from;
    
        int size = _channel->receive(buf, &from, message->data(), message->size());
        return (size > 0);
    }
    

private:
    /**
     * @brief Callback chamado pelo canal quando novos dados estão disponíveis
     */
    void update(typename Channel::Observed* obs,
                typename Channel::Observer::Observing_Condition c,
                Buffer* buf) override
    {
        Observer::update(c, buf); // Libera a thread bloqueada
    }

private:
    Channel* _channel;  // Ponteiro para o canal de comunicação
    Address _address;   // Endereço deste comunicador no canal
};
