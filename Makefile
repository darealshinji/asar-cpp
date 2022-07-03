CXX := g++
CXXFLAGS := -Wall -O3 -Irapidjson/include


all: asar

clean:
	-rm -f asar asar.exe asar.o main.o

asar: asar.o main.o
	$(CXX) -o $@ $^ $(LDFLAGS)


