all: server subscriber

server: server.cpp
	g++ -Wall -o server server.cpp -lm -lstdc++ -std=c++11

subscriber: subscriber.cpp
	g++ -Wall -o subscriber subscriber.cpp -lm -lstdc++ -std=c++11

clean:
	rm -f server subscriber

.PHONY: all clean