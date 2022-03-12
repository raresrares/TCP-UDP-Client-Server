#include "helpers.h"

using namespace std;

vector<client> clients;

void usage(char* file) {
    fprintf(stderr, "Usage: %s server_port\n", file);
    exit(0);
}

/**
 * @param buffer - unformatted message received from the udp client
 * @param ip - ip of the udp client
 * @param port - port of the udp client
 * @param recvSize - size of the received message from the udp client
 * @return datagram - datagram ready to be sent to tcp client
 */
datagram formatTopic(char* udpMessage, uint32_t ip, uint32_t port, int recvSize) {
    datagram dg;

    dg.ip.s_addr = ip;
    dg.port = port;

    /* Topic name */
    memcpy(dg.topicName, udpMessage, 50);
    dg.topicName[50] = '\0';

    /* Data type */
    dg.dataType = udpMessage[50];

    /* Topic data */
    /* 51 = 50 + 1 = sizeof(topicName) + sizeof(dataType) */
    memcpy(dg.data, udpMessage + 51, recvSize - 51);
    dg.data[recvSize - 51] = '\0';

    return dg;
}

/**
 * @return -1 if an error was encountered
 *          0 if an old client connected
 *          1 if a new client connected
 *          2 if the client is already connected
 */
int serverPreamble(int sockfd, char* id) {
    int ret;
    char buffer[BUFLEN];

    memset(buffer, 0, BUFLEN);
    ret = recv(sockfd, buffer, BUFLEN, 0);
    DIE(ret < 0, "recv");

    if (strcmp(buffer, "I would like to connect!\n")) {
        ret = sendWrongPreamble(sockfd);
        DIE(ret < 0, "send wrong preamble");

        return -1;
    }

    memset(buffer, 0, strlen(buffer));
    sprintf(buffer, "Send me your ID!\n");

    ret = send(sockfd, buffer, BUFLEN, 0);
    DIE(ret < 0, "send");

    memset(buffer, 0, strlen(buffer));
    ret = recv(sockfd, buffer, BUFLEN, 0);
    DIE(ret < 0, "recv");

    if (strncmp(buffer, "My ID is: ", 10)) {
        ret = sendWrongPreamble(sockfd);
        DIE(ret < 0, "send wrong preamble");

        return -1;
    }

    memset(id, 0, 11);
    strcpy(id, buffer + 10);

    client* c = getClientID(id);
    if (c != nullptr) {
        if (c->connected == true) {
            ret = sendClientAlreadyConn(sockfd);
            DIE(ret < 0, "send client already connected");

            return 2;
        } else {
            ret = sendClientConnected(sockfd);
            DIE(ret < 0, "send client connected");

            return 0;
        }
    } else if (c == nullptr) {
        sendClientConnected(sockfd);
        DIE(ret < 0, "send client connected");

        return 1;
    }

    return -1;
}

int sendWrongPreamble(int sockfd) {
    char buffer[BUFLEN];

    memset(buffer, 0, BUFLEN);
    sprintf(buffer, "Wrong request!\n");

    return send(sockfd, buffer, strlen(buffer), 0);
}

int sendClientAlreadyConn(int sockfd) {
    char buffer[BUFLEN];

    memset(buffer, 0, BUFLEN);
    sprintf(buffer, "Another client is connected with your ID!\n");

    return send(sockfd, buffer, BUFLEN, 0);
}

/**
 * @return client based on his sid
 */
client* getClientID(const char* id) {
    for (auto &c : clients)
        if (strcmp(c.id, id) == 0)
            return &c;

    return nullptr;
}

int sendClientConnected(int sockfd) {
    char buffer[BUFLEN];

    memset(buffer, 0, BUFLEN);
    sprintf(buffer, "You are connected!\n");

    return send(sockfd, buffer, BUFLEN, 0);
}

int main(int argc, char* argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    int ret;
    char buffer[BUFLEN];
    int tcpSocket, udpSocket, port;

    struct sockaddr_in serverAddr, clientAddr;
    socklen_t addrlen = sizeof(struct sockaddr_in);

    if (argc < 2) usage(argv[0]);

    /* Create TCP socket */
    tcpSocket = socket(PF_INET, SOCK_STREAM, 0);
    DIE(tcpSocket < 0, "TCP socket");

    /* Create UDP socket */
    udpSocket = socket(PF_INET, SOCK_DGRAM, 0);
    DIE(udpSocket < 0, "UDP socket");

    /* Port */
    port = atoi(argv[1]);
    DIE(port == 0, "atoi");


    /* Bind for TCP */
    memset((char *) &serverAddr, 0, sizeof(struct sockaddr_in));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    ret = bind(tcpSocket, (struct sockaddr *) &serverAddr, sizeof(struct sockaddr));
    DIE(ret < 0, "TCP bind");


    /* Bind for UDP */
    memset((char *) &serverAddr, 0, sizeof(struct sockaddr_in));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    ret = bind(udpSocket, (struct sockaddr *) &serverAddr, sizeof(struct sockaddr));
    DIE(ret < 0, "UDP bind");


    /* Listen for TCP */
    ret = listen(tcpSocket, MAX_CLIENTS);
    DIE(ret < 0, "TCP listen");

    fd_set read_fds;
    fd_set tmp_fds;
    int fdmax;

    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);

    FD_SET(tcpSocket, &read_fds);
    FD_SET(udpSocket, &read_fds);
    FD_SET(0, &read_fds);

    fdmax = (tcpSocket > udpSocket) ? tcpSocket : udpSocket;

    while (true) {
        tmp_fds = read_fds;

        ret = select(fdmax + 1, &tmp_fds, nullptr, nullptr, nullptr);
        DIE(ret < 0, "select");

        for (int sockid = 0; sockid <= fdmax; sockid++) {
            if (FD_ISSET(sockid, &tmp_fds)) {

                if (sockid == tcpSocket) {
                    int newSockFd = accept(tcpSocket, (struct sockaddr *) &clientAddr, &addrlen);
                    DIE(newSockFd < 0, "accept new TCP client");

                    /* Disable Nagle */
                    ret = disableNagle(newSockFd);
                    DIE(ret < 0, "disable nagle");

                    /* Update read_fds & fdmax */
                    FD_SET(newSockFd, &read_fds);
                    fdmax = (newSockFd > fdmax) ? newSockFd : fdmax;

                    char* clientID = (char *) calloc(11, sizeof(char));
                    ret = serverPreamble(newSockFd, clientID);
                    DIE(ret == -1, "server preamble");

                    /* Old Client Reconnected */
                    if (ret == 0) {
                        fprintf(stdout, "New client %s connected from %s:%d.\n",
                                clientID, inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

                        /* Update the reconnected client with the new socket and the connected status */
                        updateReconnectedClient(newSockFd, clientID);

                        client* c = getClientSocket(newSockFd);
                        datagram* dg = (datagram *) calloc(1, sizeof(datagram));

                        /* If the client was subscribed to a topic with SF = 1 send these topics */
                        for (unsigned int i = 0; i < c->sfTopics.size(); ++i) {
                            memcpy(dg, &(c->sfTopics[i]), sizeof(datagram));

                            ret = send(c->socket, dg, BUFLEN, 0);
                            DIE(ret < 0, "send sf = 1 topic");

                            memset(dg, 0, sizeof(datagram));
                        }

                        /* Empty the vector once the topics were sent */
                        c->sfTopics.clear();

                        free(dg);
                        free(clientID);
                        continue;
                    }

                    /* New Client Connected */
                    if (ret == 1) {
                        fprintf(stdout, "New client %s connected from %s:%d.\n",
                                clientID, inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

                        /* Create and add new client to the clients vector */
                        client c = createClient(newSockFd, clientID);
                        clients.push_back(c);

                        free(clientID);
                        continue;
                    }

                    /* Client already connected */
                    if (ret == 2) {
                        fprintf(stdout, "Client %s already connected.\n", clientID);

                        close(newSockFd);
                        FD_CLR(newSockFd, &read_fds);

                        continue;
                    }
                }

                if (sockid == udpSocket) {
                    memset(buffer, 0, BUFLEN);
                    ret = recvfrom(udpSocket, buffer, BUFLEN, 0, (struct sockaddr*) &clientAddr, &addrlen);
                    DIE(ret < 0, "udp recvfrom");

                    /* Format topic accordingly */
                    datagram dg = formatTopic(buffer,
                                              clientAddr.sin_addr.s_addr,
                                              ntohs(clientAddr.sin_port),
                                              ret);

                    for (unsigned int i = 0; i < clients.size(); ++i) {
                        for (unsigned int j = 0; j < clients[i].subscribedTopics.size(); ++j) {

                            if (strcmp(clients[i].subscribedTopics[j].name, dg.topicName) == 0) {

                                /* If the client is connected send the datagram */
                                if (clients[i].connected == true) {
                                    ret = send(clients[i].socket, &dg, BUFLEN, 0);
                                    DIE(ret < 0, "send topic");

                                    break;
                                }

                                /* If the client is not connected but the SF option is = 1 push
                                 * the topic to the sfTopics vector */
                                if (clients[i].connected == false &&
                                    clients[i].subscribedTopics[j].storeAndForward == true) {

                                    clients[i].sfTopics.push_back(dg);

                                    break;
                                }
                            }
                        }
                    }
                    continue;
                }

                if (sockid == 0) {
                    memset(buffer, 0, BUFLEN);
                    fgets(buffer, BUFLEN - 1, stdin);

                    /* Server received exit from stdin */
                    if (strncmp(buffer, "exit", 4) == 0) {
                        close(udpSocket);
                        close(tcpSocket);

                        /* Send an exit message to all the connected clients */
                        for (auto & client : clients) {
                            if (client.connected == true) {
                                memset(buffer, 0, BUFLEN);
                                sprintf(buffer, "Server is closing!\n");

                                ret = send(client.socket, buffer, BUFLEN, 0);
                                DIE(ret < 0, "tcp send");

                                close(client.socket);
                            }
                        }

                        return 0;
                    }

                    continue;
                } else {
                    memset(buffer, 0, BUFLEN);
                    ret = recv(sockid, buffer, BUFLEN, 0);
                    DIE(ret < 0, "tcp recv");

                    /* Client disconnected */
                    if (ret == 0 || (strcmp(buffer, "Closing the connection!\n") == 0)) {
                        /* Modify the connected parameter to false */
                        read_fds = disconnectClient(read_fds, sockid);
                    } else {
                        char *command = strtok(buffer, " \n");
                        char *topicName = strtok(nullptr, " \n");
                        char *sfString = strtok(nullptr, " \n");

                        bool sf = (sfString != nullptr) ? atoi(sfString) : 0;
                        topicName[strlen(topicName)] = '\0';

                        if (!strcmp(command, "subscribe")) subscribe(sockid, topicName, sf);

                        if (!strcmp(command, "unsubscribe")) unsubscribe(sockid, topicName);
                    }
                }
            }
        }
    }
}

/**
 * @param read_fds - the set of file descriptors currently used
 * @param sockid - socket of the client to be disconnected
 * @return a new set of file descriptors
 */
fd_set &disconnectClient(fd_set &read_fds, int sockid) {
    for (auto &client : clients) {
        if (client.socket == sockid) {
            fprintf(stdout, "Client %s disconnected.\n", client.id);
            client.connected = false;

            /* Remove the socket from read_fds */
            FD_CLR(sockid, &read_fds);
        }
    }

    return read_fds;
}

/**
 * Update the socket and the connected status of a reconnected client
 * @param newSockFd - the new socket of the client
 * @param clientID - the id of the client
 */
void updateReconnectedClient(int newSockFd, const char *clientID) {
    client* c = getClientID(clientID);

    if (c != nullptr) {
        c->socket = newSockFd;
        c->connected = true;

        return;
    }
}

client createClient(int newSockFd, const char *clientID) {
    client c;
    strcpy(c.id, clientID);
    c.socket = newSockFd;
    c.connected = true;

    return c;
}

/**
 * @return client based on his socket
 */
client* getClientSocket(int socket) {
    for (auto &c : clients)
        if (c.socket == socket)
            return &c;

    return nullptr;
}

/**
 * Subscribe a client to a topic
 * @param sockid - socket of the client
 * @param topicName - name of the topic
 * @param sf - store and forward option; either 1/0 (true/false)
 */
void subscribe(int sockid, const char *topicName, bool sf) {
    int ret;
    bool topicFound = false;
    client* cl = getClientSocket(sockid);
    DIE(cl == nullptr, "getClientSocket");

    for (unsigned int i = 0; i < (*cl).subscribedTopics.size(); ++i) {
        if (strcmp((*cl).subscribedTopics[i].name, topicName) == 0) {
            topicFound = true;

            /* If the client was previously connected with SF = 1 and the
             * new SF option is equal to 0 */
            if ((*cl).subscribedTopics[i].storeAndForward == true && sf == false) {
                datagram* dg = (datagram *) calloc(1, sizeof(datagram));

                for (unsigned int j = 0; j < cl->sfTopics.size(); ++j) {
                    memcpy(dg, &(cl->sfTopics[j]), sizeof(datagram));

                    ret = send(cl->socket, dg, BUFLEN, 0);
                    DIE(ret < 0, "send sf = 1 topic");

                    memset(dg, 0, sizeof(datagram));
                }

                cl->sfTopics.clear();
            }

            break;
        }
    }

    if (topicFound == false) {
        topic t;
        strcpy(t.name, topicName);
        t.storeAndForward = sf;

        (*cl).subscribedTopics.push_back(t);
    }

    ret = sendSubscribe(sockid);
    DIE(ret < 0, "send subscribe");
}

int sendSubscribe(int sockid) {
    char buffer[BUFLEN];

    memset(buffer, 0, BUFLEN);
    sprintf(buffer, "You are subscribed to the topic!\n");

    return send(sockid, buffer, BUFLEN, 0);
}

void unsubscribe(int sockid, char *topicName) {
    client* cl = getClientSocket(sockid);
    vector<topic>::iterator i;

    for (i = cl->subscribedTopics.begin(); i < cl->subscribedTopics.end(); i++) {
        if (strcmp(i->name, topicName) == 0) {
            cl->subscribedTopics.erase(i);
            break;
        }
    }

    int ret = sendUnsubscribe(sockid);
    DIE(ret < 0, "send unsubscribe");
}

int sendUnsubscribe(int sockid) {
    char buffer[BUFLEN];

    memset(buffer, 0, BUFLEN);
    sprintf(buffer, "You are unsubscribed from the topic!\n");

    return send(sockid, buffer, BUFLEN, 0);
}
