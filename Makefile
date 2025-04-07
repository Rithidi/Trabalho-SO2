# Makefile para o projeto de comunicação entre veículos autônomos

# Compilador e flags
CXX = g++
CXXFLAGS = -std=c++17 -pthread -Wall -Wextra -Iinclude/
LDFLAGS = -pthread

# Diretórios
SRC_DIR = src
OBJ_DIR = obj
INCLUDE_DIR = include

# Arquivo principal
MAIN_SRC = $(SRC_DIR)/main.cpp
MAIN_OBJ = $(OBJ_DIR)/main.o
MAIN_EXE = $(SRC_DIR)/main.exe  # Executável no mesmo diretório do fonte

# Regra padrão (compila apenas o main.exe)
all: $(MAIN_EXE)

# Compila o executável principal (sempre recompila o main.cpp)
$(MAIN_EXE): $(MAIN_SRC) | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $(MAIN_SRC) -o $(MAIN_OBJ)
	$(CXX) $(LDFLAGS) $(MAIN_OBJ) -o $@

# Cria o diretório de objetos se não existir
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Limpeza (remove objetos e executável)
clean:
	rm -rf $(OBJ_DIR) $(MAIN_EXE)

# Executa o programa
run: all
	./$(MAIN_EXE)

.PHONY: all clean run