#pragma once

#include "message.hpp"
#include "observer.hpp"
#include "protocol.hpp"


class Communicator : public Concurrent_Observer {
public:
    using Buffer = Protocol::Buffer;
    using Address = Protocol::Address;

public:
    Communicator(Protocol* protocol, Address address) : _protocol(protocol), _address(address) {
        _protocol->attach(this, address);
    }

    ~Communicator() {
        _protocol->detach(this, _address);
    }

    bool send(const Message* message) {
        return (_protocol->send(_address, Protocol::Address::BROADCAST, message->data(), message->size()) > 0);
    }

    bool receive(Message* message) {
        Message received_message = updated(); // block until a notification is triggered
        
        // Verifica se a mensagem recebida tem tamanho maior que zero.
        if (received_message.size() == 0) {
            return false;
        } 

        // Copia conteudo da mensagem recebida para a mensagem do comunicador.
        message->setData(received_message.data(), received_message.size());

        return true;

        /*
        Protocol::Address from;
        int size = _protocol->receive(buf, &from, message->data(), message->size());
        if (size > 0) {
            return true;
        }
        return false;
        */
    }

/*
private:
    void update(Protocol obs, int c, Buffer* buf) {
        Observer::update(c, buf); // releases the thread waiting for data
    }
*/

private:
    Protocol* _protocol;
    Address _address;
};
