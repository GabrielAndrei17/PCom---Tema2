#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <limits.h>
#include "helpers.h"


int main(int argc, char** argv) {

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	DIE(argc < 2, "< arguments");

	int enable = 1;
	int stop = 0;
	int server_port = atoi(argv[1]);

	//vector de clients
	struct client *clients = calloc(1000, sizeof(struct client));

	//socketuruile din care face stream si asculta
	int udp_sock = socket(PF_INET, SOCK_DGRAM, 0);
	int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in serv_addr, udp_addr, new_tcp;

	//initializam serv_addr
	serv_addr.sin_port = htons(server_port);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_family = AF_INET;

	//initializam udp_addr (la fel ca la serv_adr)
	udp_addr.sin_family = AF_INET;
	udp_addr.sin_port = htons(server_port);
	udp_addr.sin_addr.s_addr = INADDR_ANY;

	bind(udp_sock, (struct sockaddr *)&udp_addr, sizeof(struct sockaddr));

	bind(tcp_sock, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr));
	setsockopt(tcp_sock, IPPROTO_TCP, TCP_NODELAY, &enable, 4);

	int tcp_lsn = listen(tcp_sock, INT_MAX);
	DIE(tcp_lsn < 0, "tcp conect er");

	fd_set fd_out;
	FD_ZERO(&fd_out);
	FD_SET(tcp_sock, &fd_out);
	FD_SET(udp_sock, &fd_out);
	FD_SET(STDIN_FILENO, &fd_out);

	socklen_t udp_len = sizeof(struct sockaddr);

	int maxim_so = udp_sock;
	if(tcp_sock > udp_sock)
		maxim_so = tcp_sock;

	while(!stop) {

		char buffer[PACKSIZ];
		fd_set fd_aux = fd_out;

		int sel = select(maxim_so + 1, &fd_aux, NULL, NULL, NULL);
		DIE(sel < 0, "select");

		//parcurgem toate socketurile
		for (int i = 0; i <= maxim_so; i++) {
			if (FD_ISSET(i, &fd_aux)) {
				memset(buffer, 0, sizeof(struct Packet));
				
				if(i == udp_sock){

					int err = recvfrom(udp_sock, buffer, 1551, 0, (struct sockaddr *)&udp_addr, &udp_len);
					DIE(err < 0, "recv from udp");

					struct udp_struct *udp; //ce primim
					struct tcp_struct tcp; //ce transmitem

					//Initializam udp
					udp = (struct udp_struct *)buffer;

					//Initializam tcp la care se va trimite informatia
					tcp.port = htons(udp_addr.sin_port);
					strcpy(tcp.ip, inet_ntoa(udp_addr.sin_addr));
					strcpy(tcp.topic, udp->topic);
					tcp.topic[50] = 0;

					int num;
					double real;

					if(udp->type == 0) { //continut = numar natural

						num = ntohl(*(uint32_t *)(udp->buff + 1));

						if(udp->buff[0] == 1) //numar negativ
							num = num * (-1);

						sprintf(tcp.buff, "%d", num);
						
						strcpy(tcp.type, "INT");
					}
					else
					if (udp->type == 1) { //continut = short real
						real = abs(ntohs(*(uint16_t *)(udp->buff)));
						
						real = real / 100;

						sprintf(tcp.buff, "%.2f", real);

						strcpy(tcp.type, "SHORT_REAL");
					}
					else
					if (udp->type == 2) { //float
						int p10 = 1;
						real = ntohl(*(uint32_t *)(udp->buff + 1));

						// udp->buff[5] = numere de dupa virgula
						for(int i = 1; i <= udp->buff[5]; i++)
							p10 *= 10;
						real = real / p10;

						if(udp->buff[0] == 1) //numar negativ
							real = real * (-1);
							
						sprintf(tcp.buff, "%lf", real);

						strcpy(tcp.type, "FLOAT");
						
					} 
					else { //string
						strcpy(tcp.buff, udp->buff);

						strcpy(tcp.type, "STRING");
					}

					for(int j = 5; j <= maxim_so; j++) { //parcurgem clientii

						for(int k = 0; k < clients[j].topics_len; k++) { //topicurile la care este abonat

							if (strcmp(clients[j].topics[k].name, tcp.topic) == 0) { //daca e abonat la topicul respectiv il trimitem
								if(clients[j].online){

									//client online => ii trimitem informatia
									int ret = send(clients[j].socket, &tcp, sizeof(struct tcp_struct), 0);
									DIE(ret < 0, "send to client er");
								} 
								else {

									//daca clientul e offline ii salvam informatia in vectorul de unsent
									if(clients[j].topics[k].sf == 1) {

										clients[j].unsent[clients[j].unsent_len] = tcp;
										clients[j].unsent_len++;
									}
								}
								break;
							}
						}
					}
				}
				else if (i == tcp_sock) { //pentru socketul tcp primim informatii

					int socket = accept(tcp_sock, (struct sockaddr *) &new_tcp, &udp_len);
					DIE(socket < 0, "accept tcp er");

					recv(socket, buffer, 10, 0);

					//se cauta clientul cu id-ul dat in vectorul de clienti existenti
					int index = -1, online = 0;
					for(int j = 5; j <= maxim_so; j++) {
						if(strcmp(clients[j].id, buffer) == 0) {
							index = j;
							online = clients[j].online;
							break;
						}
					}

					//daca clientul este nou(nu a fost gasit)
					if(index == -1) {
						//adaugam socketul
						FD_SET(socket, &fd_out);
						if(socket > maxim_so)
							maxim_so = socket;

						//il adaugam la finalul vectorului
						strcpy(clients[maxim_so].id, buffer);
						clients[maxim_so].online = 1;
						clients[maxim_so].socket = socket;

						//output pentru acest caz
						printf("New client %s connected from %s:%d\n", clients[maxim_so].id,
							inet_ntoa(new_tcp.sin_addr), ntohs(new_tcp.sin_port));

					} 
					else if(index && !online) { //e gasit si nu e online
						FD_SET(socket, &fd_out);

						clients[index].online = 1;
						clients[index].socket = socket;

						printf("New client %s connected from %s:%d.\n", clients[index].id,
							inet_ntoa(new_tcp.sin_addr), ntohs(new_tcp.sin_port));
						
						//se va trimite ce a ramas netrimis cat timp a fost offline
						for(int k = 0; k < clients[index].unsent_len; k++){

							send(clients[index].socket, &clients[index].unsent[k], sizeof(struct tcp_struct), 0);
						}
						clients[index].unsent_len = 0;

					} 
					else { //deja conectat
						close(socket);
						printf("Client %s already connected.\n", clients[index].id);
					}

				} 
				
				else if (i == STDIN_FILENO) { //socketul serverului (stdin)
					
					//gestionarea functiei de oprire
					fgets(buffer, 100, stdin);

					if(strncmp(buffer, "exit", 4) == 0) {
						stop = 1;
						break;
					}

				} else { //posibili clienti noi

					int ret = recv(i, buffer, sizeof(struct Packet), 0);
					if(ret) {
						struct Packet *paket_input = (struct Packet *) buffer;
						client* c = NULL;

						for(int j = 5; j <= maxim_so; j++) {
							if (i == clients[j].socket) {
								c = &clients[j];
								break;
							}
						}

						//Analizam pachetul
						if(paket_input->type == 's') { //tip pachet s = subscribe
							int topicIndex = -1;

							//se cauta daca e abonat deja la topicul dat
							for(int j = 0; j < c->topics_len; j++) {

								if (strcmp(c->topics[j].name, paket_input->topic) == 0) {
									topicIndex = j;
									break;
								}
							}

							//daca nu e deja abonat, ne abonam la topic (il adaugam in vectorul de topics)
							if(topicIndex < 0) {

								strcpy(c->topics[c->topics_len].name, paket_input->topic);
								c->topics[c->topics_len].sf = paket_input->data_type;
								c->topics_len ++;
							}
						}
						else if(paket_input->type == 'u') { //tip pachet u = unsubscribe
							
							int topicIndex = -1;
							for(int k = 0; k < c->topics_len; k++)
								if(strcmp(c->topics[k].name, paket_input->topic) == 0) {
									topicIndex = k;
									break;
								}

							//daca clientul e abonat la topicul respectiv, ne dezabonam
							if (topicIndex >= 0) {
								for(int l = topicIndex; l < c->topics_len; l++)
									c->topics[l] = c->topics[l+1];
								c->topics_len --;
							}
						}
						else if (paket_input->type == 'e') { //tip pachet e = deconectare (exit)
							
							for (int j = 5; j <= maxim_so; j++) 
								if(clients[j].socket == i) {

									FD_CLR(i, &fd_out);
									printf("Client %s disconnected.\n", clients[j].id);
									
									clients[j].socket = -1;
									clients[j].online = 0;

									close(i);
									break;
								}
						}
					}
					 else if (ret == 0) {
						for (int j = 5; j <= maxim_so; j++)
							if(clients[j].socket == i) {

								FD_CLR(i, &fd_out);
								printf("Client %s disconnected.\n", clients[j].id);
								
								clients[j].socket = -1;
								clients[j].online = 0;
								
								close(i);
								break;
							}
					}
				}
			}
		}
	}
	for(int i = 3; i <= maxim_so; i++) {
		if(FD_ISSET(i, &fd_out))
			close(i);
	}

	return 0;
}