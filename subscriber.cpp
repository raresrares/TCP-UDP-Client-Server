#include "helpers.h"


using namespace std;

void usage(char *file) {
    fprintf(stderr, "Usage: %s <ID_Client> <IP_Server> <Port_Server>\n", file);
    exit(0);
}

/**
 * Print datagram received from server based on its datatype
 */
void printTopic(datagram dg) {
    fprintf(stdout, "%s - ", dg.topicName);

    if (dg.dataType == 0) {
        uint32_t *number = (uint32_t *) (dg.data + 1);
        uint32_t ret = ntohl(*number);

        fprintf(stdout, dg.data[0] == 1 ? "INT - -%d\n" : "INT - %d\n", ret);

        return;
    }

    if (dg.dataType == 1) {
        uint16_t* number = (uint16_t *) dg.data;
        double ret = ((double)(ntohs(*number))) / 100;

        fprintf(stdout, "SHORT_REAL - %.2f\n", ret);

        return;
    }

    if (dg.dataType == 2) {
        uint32_t *number = (uint32_t*)(dg.data + 1);
        uint8_t *power = (uint8_t*)(dg.data + 1 + sizeof(uint32_t));
        float ret = (ntohl(*number)) / pow(10, (*power));

        fprintf(stdout, dg.data[0] == 1 ? "FLOAT - -%f\n" : "FLOAT - %f\n", ret);

        return;
    }

    if (dg.dataType == 3) {
        fprintf(stdout, "STRING - %s\n", dg.data);

        return;
    }
}

/**
 * @return -1 if and error was encountered
 *          1 otherwise
 */
int subscriberPreamble(int sockfd, char * id) {
    int ret;
    char buffer[BUFLEN];


    memset(buffer, 0, BUFLEN);
    strcpy(buffer, "I would like to connect!\n");

    ret = send(sockfd, buffer, strlen(buffer), 0);
    DIE(ret < 0, "send 1");

    ret = recv(sockfd, buffer, BUFLEN, 0);
    DIE(ret < 0, "recv 1");

    if (strcmp(buffer, "Send me your ID!\n") == 1) return -1;


    memset(buffer, 0, strlen(buffer));
    sprintf(buffer, "My ID is: %s", id);

    ret = send(sockfd, buffer, strlen(buffer), 0);
    DIE(ret < 0, "send 2");

    ret = recv(sockfd, buffer, BUFLEN, 0);
    DIE(ret < 0, "recv 2");

    if (strcmp(buffer, "You are connected!\n") != 0) return -1;

    return 1;
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    int sockfd, ret;
    struct sockaddr_in servAddr;
    char buffer[BUFLEN];

    if (argc < 4) usage(argv[0]);


    /* TCP */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "socket");
    /* --- */


    /* Disable Neagle */
    int neagle = 1;
    ret = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &neagle, sizeof(neagle));
    DIE(ret < 0, "neagle");
    /* ------ */


    /* Connect to Server */
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(atoi(argv[3]));
    ret = inet_aton(argv[2], &servAddr.sin_addr);
    DIE(ret == 0, "inet_aton");

    ret = connect(sockfd, (struct sockaddr*) &servAddr, sizeof(servAddr));
    DIE(ret < 0, "connect");
    /* ---------------- */


    /* Subscriber Preamble */
    ret = subscriberPreamble(sockfd, argv[1]);
    DIE(ret < 0, "subscriber preamble");
    /* --------- */

    fd_set read_fds;
    fd_set tmp_fds;
    int fdmax;

    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);

    FD_SET(sockfd, &read_fds);
    FD_SET(0, &read_fds);

    fdmax = (sockfd > 0) ? sockfd : 0; /* mereu e egal cu sockfd oricum */

    while (true) {
        memset(buffer, 0, BUFLEN);

        tmp_fds = read_fds;

        ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
        DIE(ret < 0, "select");

        /* Receive commands from stdin */
        if (FD_ISSET(0, &tmp_fds)) {
            char aux[BUFLEN];
            memset(buffer, 0, BUFLEN);

            fgets(buffer, BUFLEN - 1, stdin);
            buffer[strcspn(buffer, "\n")] = 0; /* Remove trailing newline */
            memcpy(aux, buffer, sizeof(buffer));

            char *token;
            token = strtok(aux, " ");

            if(!strcmp(token, "exit")) {
                ret = sendClosingConnection(sockfd);
                DIE(ret < 0, "send");

                close(sockfd);
                break;
            }

            if(!strcmp(token, "subscribe") || !strcmp(token, "unsubscribe")) {
                ret = send(sockfd, buffer, sizeof(buffer), 0);
                DIE(ret < 0, "send");
            }
        }

        /* Receive from server */
        if (FD_ISSET(sockfd, &tmp_fds)) {
            memset(buffer, 0, BUFLEN);
            ret = recv(sockfd, buffer, BUFLEN, 0);
            DIE(ret < 0, "recv from server");

            if (strcmp(buffer, "Server is closing!\n") == 0) {
                close(sockfd);
                break;
            }

            if (strcmp(buffer, "You are subscribed to the topic!\n") == 0) {
                fprintf(stdout, "Subscribed to topic.\n");
                continue;
            }

            if (strcmp(buffer, "You are unsubscribed from the topic!\n") == 0) {
                fprintf(stdout, "Unsubscribed from topic.\n");
                continue;
            }

            printTopic(*((datagram *) buffer));
        }
    }

    return 0;
}

int sendClosingConnection(int sockfd) {
    char buffer[BUFLEN];

    memset(buffer, 0, BUFLEN);
    sprintf(buffer, "Closing the connection!\n");

    return send(sockfd, buffer, BUFLEN, 0);
}

