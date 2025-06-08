#include "../include/data_publisher.hpp"

// Inscreve um observador para receber mensagens de tipos específicos.
void DataPublisher::subscribe(Concurrent_Observer* obsCommunicator, std::vector<Ethernet::Type>* types) {
    std::lock_guard<std::mutex> lock(subscribers_mutex);
    subscribers[obsCommunicator] = types;
}

// Remove um observador da lista de inscritos e encerra suas threads periódicas.
void DataPublisher::unsubscribe(Concurrent_Observer* obsCommunicator) {
    {
        std::lock_guard<std::mutex> lock(subscribers_mutex);
        subscribers.erase(obsCommunicator);
    }

    delete_periodic_thread(obsCommunicator);

    std::lock_guard<std::mutex> lock(threads_mutex);
    threads.erase(obsCommunicator);
}

// Verifica quais observadores estão interessados na mensagem e os notifica.
void DataPublisher::notify(Message message) {
    Ethernet::Type msg_type = message.getType();
    Ethernet::Period period = message.getPeriod();

    std::lock_guard<std::mutex> lock(subscribers_mutex);
    for (auto& sub : subscribers) {
        for (auto& type : *(sub.second)) {
            if (type == msg_type) {
                // Envia diretamente se não for periódico
                if (period <= 0) {
                    sub.first->update(message);
                } else {
                    // Cria thread periódica para mensagens com período > 0
                    create_periodic_thread(sub.first, message);
                }
                break; // Já encontrou o tipo desejado, pula para o próximo inscrito
            }
        }
    }
}

// Cria uma thread periódica para enviar a mensagem ao observador especificado.
void DataPublisher::create_periodic_thread(Concurrent_Observer* obsCommunicator, Message message) {
    Ethernet::Period period = message.getPeriod();
    std::chrono::milliseconds period_ms(period); // Converte para milissegundos

    // Aloca variáveis de controle da thread
    auto* stop_flag = new bool(false);
    auto* cv = new std::condition_variable();
    auto* mtx = new std::mutex();

    // Cria a thread com a função de loop de envio
    std::thread t(&DataPublisher::publish_loop, this, cv, mtx, stop_flag, obsCommunicator, message, period_ms);

    // Armazena os recursos da thread
    ThreadControl control{ std::move(t), message, cv, mtx, stop_flag };

    std::lock_guard<std::mutex> lock(threads_mutex);
    threads[obsCommunicator].push_back(std::move(control));
}

// Encerra todas as threads periódicas associadas a um observador.
void DataPublisher::delete_periodic_thread(Concurrent_Observer* obsCommunicator) {
    std::lock_guard<std::mutex> lock(threads_mutex);

    for (auto& control : threads[obsCommunicator]) {
        {
            std::lock_guard<std::mutex> lock(*control.mtx);
            *control.stop_flag = true; // Sinaliza a parada da thread
        }
        control.cv->notify_all(); // Acorda a thread, se estiver dormindo
    }

    for (auto& control : threads[obsCommunicator]) {
        if (control.thread.joinable())
            control.thread.join(); // Aguarda finalização

        // Libera os recursos alocados
        delete control.cv;
        delete control.mtx;
        delete control.stop_flag;
    }
}

// Loop que roda dentro de cada thread periódica
void DataPublisher::publish_loop(std::condition_variable* cv, std::mutex* mtx, bool* stop_flag,
                                 Concurrent_Observer* obsCommunicator, Message message,
                                 std::chrono::milliseconds period_ms) {
    auto next_send = std::chrono::steady_clock::now() + period_ms;
    std::unique_lock<std::mutex> lock(*mtx);

    while (!(*stop_flag)) {
        obsCommunicator->update(message); // Envia mensagem
        next_send += period_ms;           // Define o próximo instante de envio

        // Aguarda até o próximo envio ou até ser sinalizado para parar
        cv->wait_until(lock, next_send, [&]() { return *stop_flag; });
    }
}

// Encerra todas as threads periodicas associadas a um determinado grupo.
void DataPublisher::delete_group_threads(Ethernet::Group_ID group_id) {
    std::lock_guard<std::mutex> lock(threads_mutex);
    for (auto& [obs, vector] : threads) { // Itera sobre os componentes inscritos.
        for (auto& control : vector) {  // Itera sobre as threads periodicas de cada inscrito.
            // Se for interesse externo e do grupo: finaliza thread periodica.
            if (control.message.getSrcAddress().vehicle_id != control.message.getDstAddress().vehicle_id) {
                if (control.message.getGroupID() == group_id) {
                    {
                        std::lock_guard<std::mutex> lock(*control.mtx);
                        *control.stop_flag = true; // Sinaliza a parada da thread
                    }
                    control.cv->notify_all();
                    // Libera os recursos alocados
                    delete control.cv;
                    delete control.mtx;
                    delete control.stop_flag;
                }
            }
        }
    }
}
