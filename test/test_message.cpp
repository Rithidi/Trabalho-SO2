#include "../include/message.hpp"
#include <cstring>
#include <iostream>

// Estrutura de exemplo para testar serialização
struct SensorData {
    int sensor_id;
    float temperature;
    bool status;
};

// Função para verificar igualdade de estruturas SensorData
bool sensorDataEqual(const SensorData& a, const SensorData& b) {
    return a.sensor_id == b.sensor_id &&
           a.temperature == b.temperature &&
           a.status == b.status;
}

// Teste manual para serialização/desserialização
void testStructSerialization() {
    std::cout << "=== Teste de Serializacao/Desserializacao ===" << std::endl;
    
    // 1. Criar dados de exemplo
    SensorData original;
    original.sensor_id = 42;
    original.temperature = 23.5f;
    original.status = true;

    // 2. Serializar para bytes
    Message msg;
    msg.setData(&original, sizeof(original));

    std::cout << "Tamanho da mensagem: " << msg.size() << " bytes" << std::endl;
    std::cout << "Dados serializados: [";
    for (size_t i = 0; i < sizeof(original); i++) {
        printf("%02X ", msg.data()[i]);
    }
    std::cout << "]" << std::endl;

    // 3. Desserializar de volta para estrutura
    SensorData recovered;
    std::memcpy(&recovered, msg.data(), sizeof(recovered));

    // 4. Verificar integridade
    if (sensorDataEqual(original, recovered)) {
        std::cout << "SUCESSO: Dados recuperados corretamente!" << std::endl;
        std::cout << "Sensor ID: " << recovered.sensor_id << std::endl;
        std::cout << "Temperatura: " << recovered.temperature << std::endl;
        std::cout << "Status: " << (recovered.status ? "true" : "false") << std::endl;
    } else {
        std::cout << "FALHA: Dados corrompidos!" << std::endl;
    }
}

// Teste manual para truncamento de dados grandes
void testDataTruncation() {
    std::cout << "\n=== Teste de Truncamento ===" << std::endl;
    
    // Criar um buffer maior que MAX_SIZE
    const size_t large_size = Message::MAX_SIZE + 100;
    uint8_t large_data[large_size];
    for (size_t i = 0; i < large_size; i++) {
        large_data[i] = i % 256;
    }

    Message msg;
    msg.setData(large_data, large_size);

    std::cout << "Tamanho original: " << large_size << " bytes" << std::endl;
    std::cout << "Tamanho após setData: " << msg.size() << " bytes" << std::endl;

    // Verificar truncamento
    if (msg.size() == Message::MAX_SIZE) {
        std::cout << "SUCESSO: Dados truncados corretamente para " << Message::MAX_SIZE << " bytes" << std::endl;
        
        // Verificar se os últimos bytes foram preservados
        bool data_ok = true;
        for (size_t i = 0; i < Message::MAX_SIZE; i++) {
            if (msg.data()[i] != large_data[i]) {
                data_ok = false;
                break;
            }
        }
        
        if (data_ok) {
            std::cout << "SUCESSO: Dados preservados corretamente apos truncamento" << std::endl;
        } else {
            std::cout << "FALHA: Dados corrompidos apos truncamento" << std::endl;
        }
    } else {
        std::cout << "FALHA: Dados não foram truncados corretamente" << std::endl;
    }
}

// Teste manual para mensagem vazia
void testEmptyMessage() {
    std::cout << "\n=== Teste de Mensagem Vazia ===" << std::endl;
    
    Message msg;
    
    std::cout << "Tamanho da mensagem vazia: " << msg.size() << " bytes" << std::endl;
    
    if (msg.size() == 0 && msg.data() != nullptr) {
        std::cout << "SUCESSO: Mensagem vazia criada corretamente" << std::endl;
    } else {
        std::cout << "FALHA: Problema na criação de mensagem vazia" << std::endl;
    }
}

int main() {
    // Executar todos os testes
    testStructSerialization();
    testDataTruncation();
    testEmptyMessage();
    
    return 0;
}