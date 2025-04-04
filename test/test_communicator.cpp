#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <queue>
#include <cstring>
#include <condition_variable>
#include "../include/message.hpp"
#include "../include/observer.hpp"
#include "../include/protocol.hpp"
#include "../include/communicator.hpp"

//---------------------------------------------------------
// NIC Fictícia para simulação
//---------------------------------------------------------
class NICFicticia {
public:
    using Address = int;
    using Protocol_Number = int;

    class Buffer {
    public:
        Buffer(size_t size) : _size(size) {
            _data = new uint8_t[size];
        }
        ~Buffer() {
            delete[] _data;
        }
        uint8_t* data() {
            return _data;
        }
        size_t size() const {
            return _size;
        }
    private:
        uint8_t* _data;
        size_t _size;
    };

    class Observed {
        friend class NICFicticia;
    public:
        using Protocol_Number = int;
    private:
        std::map<Protocol_Number, Concurrent_Observer<Buffer, Protocol_Number>*> _observers;
    };

    class Observer : public Concurrent_Observer<Buffer, Protocol_Number> {};

    static constexpr unsigned int MTU = 1514;

    NICFicticia(Address addr) : _address(addr) {}

    Buffer* alloc(Address, Protocol_Number, size_t size) {
        return new Buffer(size);
    }

    int send(Buffer* buf) {
        std::unique_lock<std::mutex> lock(_global_mutex);
        _global_queue.emplace(_address, buf);
        _global_cv.notify_all();
        return buf->size();
    }

    void free(Buffer* buf) {
        delete buf;
    }

    void attach(Observer* o, Protocol_Number prot) {
        _observers[prot] = o;
    }

    void detach(Observer*, Protocol_Number prot) {
        _observers.erase(prot);
    }

    static void network_loop() {
        while (true) {
            std::unique_lock<std::mutex> lock(_global_mutex);
            _global_cv.wait(lock, [] { return !_global_queue.empty(); });

            auto [from, buf] = _global_queue.front();
            _global_queue.pop();

            const Protocol<NICFicticia>::Packet* pkt = reinterpret_cast<const Protocol<NICFicticia>::Packet*>(buf->data());
            for (auto& [nic, inst] : _instances) {
                if (pkt->dst_paddr == nic || pkt->dst_paddr == Protocol<NICFicticia>::Address::BROADCAST) {
                    auto it = inst->_observers.find(Protocol<NICFicticia>::PROTO);
                    if (it != inst->_observers.end()) {
                        it->second->update(nullptr, Protocol<NICFicticia>::PROTO, new Buffer(*buf));
                    }
                }
            }

            delete buf;
        }
    }

    static void register_instance(Address addr, NICFicticia* nic) {
        _instances[addr] = nic;
    }

    Address address() const { return _address; }

private:
    Address _address;
    std::map<Protocol_Number, Observer*> _observers;

    static std::mutex _global_mutex;
    static std::condition_variable _global_cv;
    static std::queue<std::pair<Address, Buffer*>> _global_queue;
    static std::map<Address, NICFicticia*> _instances;
};

std::mutex NICFicticia::_global_mutex;
std::condition_variable NICFicticia::_global_cv;
std::queue<std::pair<NICFicticia::Address, NICFicticia::Buffer*>> NICFicticia::_global_queue;
std::map<NICFicticia::Address, NICFicticia*> NICFicticia::_instances;

// Traits (valor arbitrário para número de protocolo)
template<>
struct Traits<Protocol<NICFicticia>> {
    static constexpr int ETHERNET_PROTOCOL_NUMBER = 42;
};

//---------------------------------------------------------
// Testes
//---------------------------------------------------------

using Canal = Protocol<NICFicticia>;

void componente_loop(Canal::Address addr, Canal* canal, const std::string& nome) {
    Communicator<Canal> comm(canal, addr);
    Message msg;
    if (comm.receive(&msg)) {
        std::cout << "[" << nome << "] Recebeu: " << std::string((char*)msg.data(), msg.size()) << "\n";
    }
}

int main() {
    std::thread rede(NICFicticia::network_loop); // Thread da rede

    // Criar NICs e registrar
    NICFicticia nicA(1), nicB(2);
    NICFicticia::register_instance(1, &nicA);
    NICFicticia::register_instance(2, &nicB);

    Canal canalA(&nicA), canalB(&nicB);

    Communicator<Canal> comm1(&canalA, Canal::Address(1, 1000)); // Carro 1, porta 1000
    Communicator<Canal> comm2(&canalB, Canal::Address(2, 2000)); // Carro 2, porta 2000

    // Teste 1: Loopback
    std::thread t1([&]() {
        Message m;
        m.setData("Mensagem de loopback", 21);
        comm1.send(&m);
        Message r;
        comm1.receive(&r);
        std::cout << "[Loopback] Recebido: " << std::string((char*)r.data(), r.size()) << "\n";
    });

    // Teste 2: Comunicação entre Communicators
    std::thread t2([&]() {
        Message m;
        m.setData("Ola do carro A", 15);
        comm1.send(&m);
    });

    std::thread t3([&]() {
        Message r;
        comm2.receive(&r);
        std::cout << "[Carro B] Recebeu: " << std::string((char*)r.data(), r.size()) << "\n";
    });

    // Teste 3: Enviar várias mensagens
    std::thread t4([&]() {
        for (int i = 0; i < 5; ++i) {
            std::string texto = "Ping " + std::to_string(i);
            Message m;
            m.setData(texto.data(), texto.size());
            comm1.send(&m);
        }
    });

    std::thread t5([&]() {
        for (int i = 0; i < 5; ++i) {
            Message r;
            comm2.receive(&r);
            std::cout << "[Carro B - Ping] " << std::string((char*)r.data(), r.size()) << "\n";
        }
    });

    // Teste 4: Comunicação entre componentes de carros diferentes (threads)
    std::thread sensor([&]() {
        componente_loop(Canal::Address(1, 1234), &canalA, "Sensor - Carro A");
    });

    std::thread atuador([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        Communicator<Canal> comm(&canalA, Canal::Address(1, 1234));
        Message msg;
        msg.setData("Comando: frear", 15);
        comm.send(&msg);
    });

    t1.join(); t2.join(); t3.join(); t4.join(); t5.join();
    sensor.join(); atuador.join();
    rede.detach(); // Finaliza thread da rede (em produção teria sinalização)

    return 0;
}
