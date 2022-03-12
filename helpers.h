#ifndef TEMA2_HELPERS_H
#define TEMA2_HELPERS_H

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <list>
#include <iostream>
#include <cmath>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <queue>

#define BUFLEN sizeof(datagram)
#define MAX_CLIENTS	5

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

typedef struct Client client;
typedef struct Topic topic;
typedef struct UDPTopic datagram;

typedef struct UDPTopic {
    struct in_addr ip;
    uint32_t port;

    char topicName[51];
    uint32_t dataType;
    char data[1501];
} datagram;

typedef struct Client {
    char id[11];
    int socket;

    bool connected;

    std::vector<topic> subscribedTopics;
    std::vector<datagram> sfTopics;
} client;

typedef struct Topic {
    char name[51];
    bool storeAndForward;
} topic;

int disableNagle(int newSockFd);
void subscribe(int sockid, const char *topicName, bool sf);
void unsubscribe(int sockid, char *topicName);
client createClient(int newSockFd, const char *clientID);
void updateReconnectedClient(int newSockFd, const char *clientID);
fd_set &disconnectClient(fd_set &read_fds, int sockid);

int sendSubscribe(int sockid);
int sendClientAlreadyConn(int sockfd);
int sendUnsubscribe(int sockid);
int sendClosingConnection(int sockfd);
int sendClientConnected(int sockfd);
int sendWrongPreamble(int sockfd);

client * getClientSocket(int socket);
client * getClientID(const char* id);

int disableNagle(int newSockFd) {
    int neagle = 1;

    return setsockopt(newSockFd, IPPROTO_TCP, TCP_NODELAY, (char *) &neagle, sizeof(int));
}

#endif
