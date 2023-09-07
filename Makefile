CXX := g++
CXXFLAGS := -Wall -O3 -Irapidjson/include -std=c++11


all: asar

clean:
	-rm -f asar asar.exe asar.o main.o

asar: asar.o main.o
	$(CXX) -o $@ $^ $(LDFLAGS)


