# Compilador
CXX := g++

# Flags de compilação
CXXFLAGS := -std=c++17 -pthread -I./include
LDFLAGS := -pthread

# Diretórios
SRC_DIR := ./src
INC_DIR := ./include
TARGET := main.exe

# Lista todos os arquivos .cpp
SRCS := $(wildcard $(SRC_DIR)/*.cpp)

# Interface de rede (pode ser passada como argumento ao make)
NETWORK_INTERFACE ?= "lo"

# Número total de mensagens (pode ser passado como argumento ao make)
TOTAL_MESSAGES ?= 1000

# Regra principal: compila diretamente para o executável
$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -DNETWORK_INTERFACE=$(NETWORK_INTERFACE) -DTOTAL_MESSAGES=$(TOTAL_MESSAGES) $^ -o $@
	@echo "Executável criado: $(TARGET)"

# Limpeza
clean:
	rm -f $(TARGET)
	@echo "Executável removido"

.PHONY: all clean

all: $(TARGET)