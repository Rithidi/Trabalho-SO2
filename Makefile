# Compilador e flags
CXX := g++
CXXFLAGS := -std=c++17 -pthread -I./include
LDFLAGS := -pthread

# Diretórios
SRC_DIR := ./src
TEST_DIR := ./test

# Arquivos-fonte do projeto principal
SRC_FILES := $(wildcard $(SRC_DIR)/*.cpp)

# Lista de testes (adicione aqui os nomes dos arquivos de teste sem .cpp)
TESTS := internal_communication_test external_communication_test

# Regra principal: compila todos os testes
all: $(TESTS)

# Regra para compilar cada teste individual
$(TESTS): %: $(TEST_DIR)/%.cpp $(SRC_FILES)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@
	@echo "Executável criado: $@"

# Limpa os executáveis de teste
clean:
	rm -f $(TESTS)
	@echo "Executáveis de teste removidos"

.PHONY: all clean $(TESTS)
