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

# Regra principal: compila diretamente para o executável
$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@
	@echo "Executável criado: $(TARGET)"

# Limpeza
clean:
	rm -f $(TARGET)
	@echo "Executável removido"

.PHONY: all clean

all: $(TARGET)