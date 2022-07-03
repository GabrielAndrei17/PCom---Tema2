#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include "helpers.h"


int main(int argc, char** argv) {

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	DIE(argc < 4, "< arguments");
	int enable = 1;
	int port = atoi(argv[3]);

	int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
	DIE(tcp_sock < 0, "socket er");

	struct sockaddr_in server_data;
	server_data.sin_port = htons(port);
	server_data.sin_family = AF_INET;
	
	inet_aton(argv[2], &server_data.sin_addr);

	fd_set fd_out;
	FD_ZERO(&fd_out);
	FD_SET(tcp_sock, &fd_out);
	FD_SET(STDIN_FILENO, &fd_out);

	//Se conecteaza la server prin socketul tcp

	int	ret = connect(tcp_sock, (struct sockaddr *)&server_data, sizeof(server_data));
	DIE(ret < 0, "connect server");

	//Se trimite ip-ul
	
	ret = send(tcp_sock, argv[1], 10, 0);
	DIE(ret < 0, "send1");

	//pachetul care va fi trimis la server
	struct Packet pack;
	setsockopt(tcp_sock, IPPROTO_TCP, TCP_NODELAY, (char *)&enable, sizeof(int));

	//Gestionam inputurile primite de la server sau din stdin
	while(1) {
		fd_set fd_aux = fd_out;

		select(tcp_sock + 1, &fd_aux, NULL, NULL, NULL);

		if (FD_ISSET(STDIN_FILENO, &fd_aux)) { //input stdin
			
			char buffer[100];
			fgets(buffer, 100, stdin);

			if(strncmp(buffer, "subscribe", 9) == 0) { //subscribe
				
				pack.type = 's';

				char *string = strtok(buffer, " ");
				string = strtok(NULL, " ");
				strcpy(pack.topic, string);

				string = strtok(NULL, " ");
				pack.data_type = string[0] - '0';

				int sen = send(tcp_sock, &pack, sizeof(struct Packet), 0);
				DIE (sen < 0, "send2");

				printf("Subscribed to topic.\n");
			}
			else if (strncmp(buffer, "unsubscribe", 11) == 0) { //unsubscribe

				pack.type = 'u';

				char *string = strtok(buffer, " ");
				string = strtok(NULL, " ");
				strcpy(pack.topic, string);

				string = strtok(NULL, " ");
				pack.data_type = string[0] - '0';

				int sen = send(tcp_sock, &pack, sizeof(struct Packet), 0);
				DIE (sen < 0, "send3");

				printf("Unsubscribed to topic.\n");
			}
			else if(strncmp(buffer, "exit", 4) == 0) { //deconectare

				pack.type = 'e';

				//Se trimite pachetul de iesire
				int sen = send(tcp_sock, &pack, sizeof(struct Packet), 0);
				DIE (sen < 0, "send2");
				break;
			}
			else printf("Input invalid.\n");
		}

		if(FD_ISSET(tcp_sock, &fd_aux)) { //informatie primita de la server

			char buffer[TCPSIZ];

			int recv1 = recv(tcp_sock, buffer, sizeof(struct tcp_struct), 0);

			if(recv1 == 0)
				break;

			struct tcp_struct *pack_send = (struct tcp_struct *)buffer;
			printf("%s:%u - %s - %s - %s\n", pack_send->ip, pack_send->port, pack_send->topic, pack_send->type, pack_send->buff);
		}
	}

	close(tcp_sock);
	return 0;
}