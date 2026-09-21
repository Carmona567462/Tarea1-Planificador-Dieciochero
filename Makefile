CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17
LDFLAGS = -lpthread

all: planificador

planificador: main.cpp
	$(CXX) $(CXXFLAGS) -o planificador main.cpp $(LDFLAGS)

clean:
	rm -f planificador
