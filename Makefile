CXX = g++
CXXFLAGS = -Wall -std=c++17 -pthread -Iinclude
SRC = src/main.cpp
TARGET = bin/main

all: $(TARGET)

$(TARGET): $(SRC)
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) $^ -o $@

run: all
	./$(TARGET)

clean:
	rm -rf bin
