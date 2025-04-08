

#include <tuple>
#include <list>
#include <mutex>
#include <vector>

// Forward declaration of Concurrent_Observer
class Concurrent_Observer {
public:
    void update(const std::vector<uint8_t>& data);
};

// Tupla de observador e condição de observação (numero da porta)
using ObserverEntry = std::tuple<Concurrent_Observer*, int>;
class Concurrent_Observed {
    public:
        // Adiciona um observador à lista de observadores
        void attach(Concurrent_Observer* observer, int port) {
            mutex.lock();
            // Adciona tupla (observador, numero da porta) na lista de observadores
            observers.emplace_back(observer, port);
            mutex.unlock();
        }

        // Remove um observador da lista de observadores
        void detach(Concurrent_Observer* observer) {
            mutex.lock();
            // Remove o observador da lista com mesmo numero de porta
            for (auto it = observers.begin(); it != observers.end(); ) {
                if (std::get<0>(*it) == observer) {
                    it = observers.erase(it);
                } else {
                    ++it;
                }
            }
            mutex.unlock();
        }

        // Notifica todos os observadores com o mesmo numero de porta
        void notify(int port, const std::vector<uint8_t>& data) {
            mutex.lock();
            for (const auto& entry : observers) {
                if (std::get<1>(entry) == port) {
                    std::get<0>(entry)->update(data); // Chama o método update do observador
                }
            }
            mutex.unlock();
        }
    
    private:
        std::list<ObserverEntry> observers;
        std::mutex mutex;

};


class Conditional_Data_Observer;


class Conditional_Data_Observed;