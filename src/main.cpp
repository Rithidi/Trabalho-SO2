#include <iostream>
#include <thread>
#include <chrono>
#include "concurrent_observer.h"

using namespace std::chrono_literals;

using Data = int;
using Condition = int;

void producer(Concurrent_Observed<Data, Condition>* observed) {
    for (int i = 0; i < 5; ++i) {
        std::this_thread::sleep_for(500ms);
        Data* d = new Data(i);
        observed->notify(0, d);
        std::cout << "[Producer] Sent: " << *d << std::endl;
    }
}

void consumer(Concurrent_Observer<Data, Condition>* observer) {
    for (int i = 0; i < 5; ++i) {
        Data* d = observer->updated();
        std::cout << "[Consumer] Received: " << *d << std::endl;
        delete d;
    }
}

int main() {
    Concurrent_Observed<Data, Condition> observed;
    Concurrent_Observer<Data, Condition> observer;

    observed.attach(&observer, 0);

    std::thread prod(producer, &observed);
    std::thread cons(consumer, &observer);

    prod.join();
    cons.join();

    return 0;
}
