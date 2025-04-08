CXX = g++
CXXFLAGS = -Wall -std=c++17 -pthread -Iinclude
SRC = src/main.cpp
OUT = bin/main

all:
	mkdir -p bin
	$(CXX) $(CXXFLAGS) $(SRC) -o $(OUT)

clean:
	rm -rf bin/*
