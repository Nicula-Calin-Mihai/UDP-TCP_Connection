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

typedef struct UDPTopic {
    struct in_addr ip;
    uint32_t port;

    char topicName[51];
    uint32_t dataType;
    char data[1501];
} datagram;

typedef struct Topic {
    char name[51];
    bool sf;
} topic;

typedef struct Client {
    char id[11];
    int socket;

    bool connected;

    std::vector<topic> subscribedTopics;
    std::vector<datagram> sfTopics;
} client;


int disableNagle(int newsockfd) {
    int neagle = 1;

    return setsockopt(newsockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &neagle, sizeof(int));
}

#endif
