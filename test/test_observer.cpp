#include "../include/observer.hpp" // Inclui suas classes Observer/Observed
#include <iostream>
#include <thread>
#include <vector>
#include <memory>
#include <cassert>

// Teste 1: Verificação básica do padrão Observer
void test_basic_observer_pattern() {
    std::cout << "\n=== TESTE BÁSICO DO OBSERVER ===" << std::endl;
    
    Concurrent_Observed<int, int> subject;
    Concurrent_Observer<int, int> observer;
    
    // Registra o observer para eventos com condição 1
    subject.attach(&observer, 1);
    
    // Thread para enviar notificação
    std::thread notifier([&]() {
        std::cout << "Thread de notificação criando dado..." << std::endl;
        int* data = new int(42);
        std::cout << "Notificando observers..." << std::endl;
        subject.notify(1, data);
    });
    
    // Recebe o dado (bloqueante)
    std::cout << "Aguardando notificação..." << std::endl;
    auto received = observer.updated();
    
    std::cout << "Valor recebido: " << *received << std::endl;
    assert(*received == 42 && "Erro: Valor recebido diferente do esperado!");
    
    notifier.join();
    std::cout << "Teste básico concluído com sucesso!\n" << std::endl;
}

// Teste 2: Verificação de comportamento concorrente
void test_concurrent_access() {
    std::cout << "\n=== TESTE DE CONCORRÊNCIA ===" << std::endl;
    
    Concurrent_Observed<std::string, int> subject;
    Concurrent_Observer<std::string, int> observer1, observer2;
    
    // Registra observers para diferentes condições
    subject.attach(&observer1, 1);
    subject.attach(&observer2, 2);
    
    const int NUM_MENSAGENS = 100;
    std::cout << "Iniciando teste com " << NUM_MENSAGENS << " mensagens por thread..." << std::endl;
    
    // Threads produtoras
    std::thread t1([&]() {
        for (int i = 0; i < NUM_MENSAGENS; ++i) {
            subject.notify(1, new std::string("Thread1-" + std::to_string(i)));
        }
    });
    
    std::thread t2([&]() {
        for (int i = 0; i < NUM_MENSAGENS; ++i) {
            subject.notify(2, new std::string("Thread2-" + std::to_string(i)));
        }
    });
    
    t1.join();
    t2.join();
    
    // Verifica se recebeu todas as mensagens
    int count1 = 0, count2 = 0;
    while (!observer1.empty()) {
        auto msg = observer1.updated();
        count1++;
        // Verifica o padrão das mensagens
        assert(msg->find("Thread1-") != std::string::npos && "Mensagem incorreta para observer1!");
    }
    
    while (!observer2.empty()) {
        auto msg = observer2.updated();
        count2++;
        // Verifica o padrão das mensagens
        assert(msg->find("Thread2-") != std::string::npos && "Mensagem incorreta para observer2!");
    }
    
    std::cout << "Resultados:" << std::endl;
    std::cout << " - Observer1 recebeu " << count1 << " mensagens" << std::endl;
    std::cout << " - Observer2 recebeu " << count2 << " mensagens" << std::endl;
    
    assert(count1 == NUM_MENSAGENS && "Erro: Observer1 não recebeu todas as mensagens!");
    assert(count2 == NUM_MENSAGENS && "Erro: Observer2 não recebeu todas as mensagens!");
    
    std::cout << "Teste de concorrência concluído com sucesso!\n" << std::endl;
}

// Teste 3: Verificação de detach e múltiplos observers
void test_multiple_observers() {
    std::cout << "\n=== TESTE DE MÚLTIPLOS OBSERVERS ===" << std::endl;
    
    Concurrent_Observed<double, int> subject;
    Concurrent_Observer<double, int> observerA, observerB, observerC;
    
    // Registra observers
    subject.attach(&observerA, 1);
    subject.attach(&observerB, 1); // Mesma condição que A
    subject.attach(&observerC, 2); // Condição diferente
    
    // Envia notificações
    subject.notify(1, new double(3.14));
    subject.notify(2, new double(2.71));
    
    // Verifica recebimento
    auto valA = observerA.updated();
    auto valB = observerB.updated();
    auto valC = observerC.updated();
    
    std::cout << "Valores recebidos:" << std::endl;
    std::cout << " - ObserverA: " << *valA << std::endl;
    std::cout << " - ObserverB: " << *valB << std::endl;
    std::cout << " - ObserverC: " << *valC << std::endl;
    
    assert(*valA == 3.14 && "Erro no valor do ObserverA");
    assert(*valB == 3.14 && "Erro no valor do ObserverB");
    assert(*valC == 2.71 && "Erro no valor do ObserverC");
    
    // Remove observerB e testa novamente
    subject.detach(&observerB, 1);
    subject.notify(1, new double(1.41));
    
    valA = observerA.updated();
    assert(observerB.empty() && "ObserverB deveria estar vazio após detach");
    assert(*valA == 1.41 && "Erro no novo valor do ObserverA");
    
    std::cout << "Teste de múltiplos observers concluído com sucesso!\n" << std::endl;
}

int main() {
    std::cout << "INICIANDO TESTES DA IMPLEMENTAÇÃO OBSERVER/OBSERVED\n" << std::endl;
    
    test_basic_observer_pattern();
    test_concurrent_access();
    test_multiple_observers();
    
    std::cout << "TODOS OS TESTES FORAM CONCLUÍDOS COM SUCESSO!" << std::endl;
    return 0;
}