#include "helpers.h"

using namespace std;

void usage(char *file)
{
	cout << "Usage: "<< file <<"server_port\n";
	exit(0);
}

vector<client> clients;

client* getClientSocket(int socket) { // Cauta clientul dupa socket
	for (auto &c : clients)
		if (c.socket == socket)
			return &c;
	return NULL;
}

client* getClientID(const char* id) { // cauta clientul dupa ID
	for (auto &c : clients)
		if (!strcmp(c.id, id))
			return &c;
	return NULL;
}

void subscribe(int idx, const char *topicName, bool sf) {
	int ret;
	unsigned int i;
	client* cl = getClientSocket(idx); // gaseste clientul
	DIE(cl == NULL, "getClientSocket");
	bool topicFound = false;

	for (i = 0; i < (*cl).subscribedTopics.size(); i++) {
		if (!strcmp((*cl).subscribedTopics[i].name, topicName)) { // gaseste topicul
			topicFound = true;

			// sf era 1, dar noul sf e 0
			if ((*cl).subscribedTopics[i].sf && !sf) {
				datagram* dg = (datagram *) calloc(1, sizeof(datagram));

				for (auto sft : cl->sfTopics) { // ultim mesaj
					memset(dg, 0, sizeof(datagram));
					memcpy(dg, &sft, sizeof(datagram));

					ret = send(cl->socket, dg, BUFLEN, 0);
					DIE(ret == -1, "send sft 1");
				}
				cl->sfTopics.clear();
			}
			break;
		}
	}
	if (!topicFound) { // nu l a gasit
		topic t;
		strcpy(t.name, topicName);
		t.sf = sf;

		(*cl).subscribedTopics.push_back(t); // il adauga
	}

	char aux[BUFLEN];

	memset(aux, 0, BUFLEN);
	sprintf(aux, "You are subscribed to the topic!\n");

	ret = send(idx, aux, BUFLEN, 0); // mesaj de confirmare
	DIE(ret < 0, "send subscribe");
}

void unsubscribe(int idx, char *topicName) {
	int ret;
	client* cl = getClientSocket(idx); // gaseste clientul

	for (unsigned int i = 0; i < cl->subscribedTopics.size(); i++) { // cauta
		if (!strcmp(cl->subscribedTopics[i].name, topicName)) { // gaseste
			cl->subscribedTopics.erase(cl->subscribedTopics.begin() + i); // elimina
			break;
		}
	}

	char aux[BUFLEN];
	memset(aux, 0, BUFLEN);
	sprintf(aux, "You are unsubscribed from the topic!\n");
	ret = send(idx, aux, BUFLEN, 0); // confirma
	DIE(ret < 0, "send unsubscribe");
}

int attached(char* id, int sockfd) {
	int ret;
	memset(id, 0, 11);
	char buffer[BUFLEN];
	memset(buffer, 0, BUFLEN);
	ret = recv(sockfd, buffer, BUFLEN, 0); // preia ID
	DIE(ret == -1, "recv");
	strcpy(id, buffer);
	client* c = getClientID(id); // cauta clientul

	if (c) { // este in lista
		if (c->connected) { // este deja conectat
			memset(buffer, 0, BUFLEN);
			sprintf(buffer, "Another client is connected with your ID!\n");
			send(sockfd,buffer, strlen(buffer), 0);
			return 2;
		} else { // reconectare
			memset(buffer, 0, BUFLEN);
			sprintf(buffer, "x\n");
			send(sockfd,buffer, strlen(buffer), 0);
			return 0;
		}
	} else { // nou conectat
		memset(buffer, 0, BUFLEN);
		sprintf(buffer, "x\n");
		send(sockfd,buffer, strlen(buffer), 0);
		return 1;
	}
}

int main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	int tcp_socket, udp_socket, portno, newsockfd;
	portno = atoi(argv[1]);
	DIE(portno == 0, "atoi");
	char buffer[BUFLEN];
	struct sockaddr_in serv_addr, cli_addr;
	serv_addr.sin_family = AF_INET;
	int ret;
	socklen_t clilen = sizeof(struct sockaddr_in);


	fd_set read_fds;	// memoria
	fd_set tmp_fds;		// se modifica
	int fdmax;			// valoare maxima din read_fds

	if (argc < 2) {
		usage(argv[0]);
	}

	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	tcp_socket = socket(PF_INET, SOCK_STREAM, 0);
	DIE(tcp_socket == -1, "TCP socket");


	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);

	ret = bind(tcp_socket, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	DIE(ret == -1, "TCP bind");

	memset((char *) &serv_addr, 0, clilen);
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	udp_socket = socket(PF_INET, SOCK_DGRAM, 0);
	DIE(udp_socket == -1, "UDP socket");

	ret = bind(udp_socket, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	DIE(ret == -1, "UDP bind");

	ret = listen(tcp_socket, MAX_CLIENTS);
	DIE(ret == -1, "TCP listen");

	FD_SET(tcp_socket, &read_fds); // se adauga subscriber
	FD_SET(udp_socket, &read_fds); // se adauga contentCreator
	FD_SET(0, &read_fds); // aici vorbeste serverul singur

	fdmax = (tcp_socket > udp_socket) ? tcp_socket : udp_socket;

	tmp_fds = read_fds;
	while (1) {
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret == -1, "select");

		for (int idx = 0; idx <= fdmax; idx++) {
			if (FD_ISSET(idx, &tmp_fds)) { // fd valid
				if (idx == tcp_socket) {
					newsockfd = accept(tcp_socket, (struct sockaddr *) &cli_addr, &clilen);
					DIE(newsockfd == -1, "TCP accept");

					ret = disableNagle(newsockfd);
					DIE(ret == -1, "disable nagle");

					FD_SET(newsockfd, &read_fds);

					if (newsockfd > fdmax) {
						fdmax = newsockfd;
					}

					char* clientID = (char *) malloc(11 * sizeof(char));
					ret = attached(clientID, newsockfd); // ia ID ul
					DIE(ret < 0, "attached");

					if (!ret) { // daca este reconectat
						cout << "New client "<<clientID<<" connected from " << inet_ntoa(cli_addr.sin_addr)<< ":" << ntohs(cli_addr.sin_port) <<".\n";

						client* cli = getClientID(clientID); // il gaseste

						if (cli) { // il innoieste
							cli->socket = newsockfd;
							cli->connected = true;
						}

						client *c = getClientSocket(newsockfd); // il ia
						datagram *dg = (datagram *) calloc(1, sizeof(datagram));

						// isi primeste feed ul de la topicurile la care era cu sf = 1
						for (auto sft : c->sfTopics) {
							memcpy(dg, &(sft), sizeof(datagram));

							ret = send(c->socket, dg, BUFLEN, 0);
							DIE(ret == -1, "send sf = 1 topic");

							memset(dg, 0, sizeof(datagram));
						}

						c->sfTopics.clear(); // curata inbox-ul

						free(dg);
						free(clientID);
						continue;
					}
					/* New Client Connected */
					if (ret == 1) {
						cout << "New client "<< clientID <<" connected from "<<inet_ntoa(cli_addr.sin_addr)<<":"<<ntohs(cli_addr.sin_port)<<".\n";
						/* Create and add new client to the clients vector */
						client c;
						strcpy(c.id, clientID);
						c.socket = newsockfd;
						c.connected = true;
						clients.push_back(c);

						free(clientID);
						continue;
					}

					/* Client already connected */
					if (ret == 2) {
						cout << "Client "<< clientID <<" already connected.\n";

						close(newsockfd);
						FD_CLR(newsockfd, &read_fds);
						continue;
					}
				}
				if (idx == udp_socket) {
					memset(buffer, 0, BUFLEN);
					ret = recvfrom(udp_socket, buffer, BUFLEN, 0, (struct sockaddr*) &cli_addr, &clilen);
					DIE(ret == -1, "UDP recvfrom");

					int counter = 50; // cu asta tin minte cati octeti am parcurs
					datagram dg; // formez mesajul
					memcpy(dg.topicName, buffer, counter);
					dg.topicName[counter] = '\0';
					dg.dataType = buffer[counter++];
					memcpy(dg.data, buffer + counter, ret - counter);
					dg.data[ret - counter] = '\0';
					dg.ip.s_addr = cli_addr.sin_addr.s_addr;
					dg.port = ntohs(cli_addr.sin_port);

					unsigned int i,j;
					for (i = 0; i < clients.size(); i++) { // parcurg clientii
						for (j = 0; j < clients[i].subscribedTopics.size(); j++) { // parcurg abonamentele clientului

							if (!strcmp(clients[i].subscribedTopics[j].name, dg.topicName)) { // daca l a gasit
								if (clients[i].connected) { // daca e conectat
									ret = send(clients[i].socket, &dg, BUFLEN, 0);
									DIE(ret < 0, "send topic");

									break;
								}

								if (!clients[i].connected &&
								    clients[i].subscribedTopics[j].sf) { // daca nu e conectat ii baga in inbox
									clients[i].sfTopics.push_back(dg);
									break;
								}
							}
						}
					}
					continue;
				}
				if (!idx) { // input in server
					memset(buffer, 0, BUFLEN);
					fgets(buffer, BUFLEN - 1, stdin);

					if (!strncmp(buffer, "exit", 4)) { // inchide serverul
						close(udp_socket);
						close(tcp_socket);

						// trimite mesaj ca se inchide
						for (auto & client : clients) {
							if (client.connected) {
								memset(buffer, 0, BUFLEN);
								strcpy(buffer, "Server is closing!\n");

								ret = send(client.socket, buffer, BUFLEN, 0);
								DIE(ret == -1, "tcp send");

								close(client.socket);
							}
						}

						break;
					}
				}
				else {
					memset(buffer, 0, BUFLEN);
					ret = recv(idx, buffer, BUFLEN, 0);
					DIE(ret < 0, "tcp recv");

					// clientul a dat comanda exit
					if (!(strcmp(buffer, "Closing the connection!\n") || !ret)) {
						for (auto &client : clients) { // cauta clientul
							if (client.socket == idx) { // il gaseste
								cout <<"Client " << client.id << " disconnected.\n"; // mesaj in server
								client.connected = false; // trece pe false conectivitatea
								FD_CLR(idx, &read_fds); // ii sterge socket ul, dar clientul ramane in lista
							}
						}
					} else { // subscribe / unsubscribe sau altceva
						char *command = strtok(buffer, " \n");
						char *topicName = strtok(NULL, " \n");
						char *sfString = strtok(NULL, " \n");

						bool sf = (sfString) ? atoi(sfString) : false; // daca exista flag il ia
						topicName[strlen(topicName)] = '\0';

						if (!strcmp(command, "subscribe")) {
							subscribe(idx, topicName, sf);
						}

						if (!strcmp(command, "unsubscribe")) {
							unsubscribe(idx, topicName);
						}
					}
				}
			}
		}
		tmp_fds = read_fds; // refresh
	}
	return 0;
}
