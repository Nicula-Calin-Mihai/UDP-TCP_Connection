#include "helpers.h"

using namespace std;

void usage(char *file) {
	cout << "Usage: "<< file << "<ID_Client> <IP_Server> <Port_Server>\n";
	exit(0);
}

void printTopic(datagram dg) {
	cout << dg.topicName <<" - ";

	if (!dg.dataType) { // 0
		uint32_t ret = ntohl(*((uint32_t *) (dg.data + 1)));

		if (dg.data[0] == 1) {
			cout << "INT - -" << ret <<"\n";
		}
		else {
			cout << "INT - "  << ret <<"\n";
		}

	}

	if (dg.dataType == 3) { // 3
		cout << "STRING - " << dg.data << "\n";

	}

	if (dg.dataType == 1) { // 1
		printf( "SHORT_REAL - %.2f\n", ((double)(ntohs(*(uint16_t *) dg.data))) / 100);
	}

	if (dg.dataType == 2) { // 2
		double ret = (ntohl(*(uint32_t*)(dg.data + 1))) / pow(10, (*(uint8_t*)(dg.data + 1 + sizeof(uint32_t))));
		if (dg.data[0] == 1) {
			printf("FLOAT - -%f\n", ret);
		}
		else {
			printf("FLOAT - %f\n", ret);
		}
	}
}

int attach(int sockfd, char* id) {
	int ret;
	char buffer[BUFLEN];

	memset(buffer, 0, BUFLEN);
	strcpy(buffer, id);

	ret = send(sockfd, buffer, strlen(buffer), 0);
	DIE(ret < 0, "send");

	ret = recv(sockfd, buffer, strlen(buffer), 0);
	if (strncmp(buffer,"x",1)) { // In cazul in care este deja abonat
		return -1;
	}
	return 1;
}

int main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	char buffer[BUFLEN];
	fd_set read_fds, tmp_fds; // read_fds retine fds, iar tmp_fds poate fi modificat
	struct sockaddr_in serv_addr;
	serv_addr.sin_family = AF_INET;
	int sockfd, ret;

	if (argc < 4) {
		usage(argv[0]);
	}

	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	serv_addr.sin_port = htons(atoi(argv[3]));
	DIE(ret == 0, "inet_aton");

	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");

	// Se seteaza in read_fds noile fds, adica 0 si socketul acestui subscriber
	FD_SET(STDIN_FILENO, &read_fds);
	FD_SET(sockfd, &read_fds);

	// Disable Nagle
	ret = disableNagle(sockfd);
	DIE(ret < 0, "nagle");

	ret = attach(sockfd, argv[1]);
	DIE(ret < 0, "attach");
	memset(buffer, 0, BUFLEN);
	while (1) {
		tmp_fds = read_fds; // aici il repar pe tmp_fds

		ret = select(sockfd + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		if (FD_ISSET(STDIN_FILENO, &tmp_fds)) { // pentru input

			char aux[BUFLEN]; // nu vreau sa pierd buffer-ul
			memset(buffer, 0, BUFLEN);

			fgets(buffer, BUFLEN - 1, stdin);
			buffer[strlen(buffer)-1] = '\0'; // scot 'ENTER'
			memcpy(aux, buffer, sizeof(buffer));

			char *token;
			token = strtok(aux, " "); // extrag primul cuvant

			if (!strcmp(token, "exit")) {
				memset(buffer, 0, BUFLEN);
				sprintf(buffer, "Closing the connection!\n");
				send(sockfd, buffer, BUFLEN, 0);
				break;
			}

			if(!strcmp(token, "unsubscribe") || !strcmp(token, "subscribe")) {
				ret = send(sockfd, buffer, strlen(buffer), 0);
				DIE(ret < 0, "send");
			}
		}

		if (FD_ISSET(sockfd, &tmp_fds)) { // pentru socket
			memset(buffer, 0, BUFLEN);
			ret = recv(sockfd, buffer, BUFLEN, 0);
			DIE(ret < 0, "recv");

			if (!strcmp(buffer, "Server is closing!\n")) { // exit de la server
				close(sockfd);
				break;
			}

			if (!strcmp(buffer, "You are unsubscribed from the topic!\n")) { // confirmare
				fprintf(stdout, "Unsubscribed from topic.\n");
				continue;
			}

			if (!strcmp(buffer, "You are subscribed to the topic!\n")) { // confirmare
				fprintf(stdout, "Subscribed to topic.\n");
				continue;
			}
			printTopic(*((datagram *) buffer)); // afiseaza mesajul primit
		}
		memset(buffer, 0, BUFLEN); // curata buffer ul
	}

	close(sockfd); // inchide legatura

	return 0;
}
