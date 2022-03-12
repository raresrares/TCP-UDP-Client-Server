CXXFLAGS = -Wall -g

all: server subscriber

# Compileaza server.cpp
server: server.cpp

# Compileaza subscriber.cpp
subscriber: subscriber.cpp

.PHONY: clean run_server run_client

clean:
	rm -f server subscriber
