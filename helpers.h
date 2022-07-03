#ifndef _HELPERS_H
#define _HELPERS_H 1

#define PACKSIZ sizeof(struct Packet)
#define TCPSIZ sizeof(struct tcp_struct)
#define UDPSIZ sizeof(struct udp_struct)

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

typedef struct topic{
	char name[51];
	int sf;
} topic;

//trimisa de client udp la server
typedef struct udp_struct {

	char topic[50];
	uint8_t type;
	char buff[1501];
} msg_udp;

//transmisa de server spre client tcp
typedef struct tcp_struct {

	char ip[16];
	uint16_t port;

	char topic[51];
	char type[11];
	char buff[1501];
} msg_tcp;

//trimis subscriber la server
typedef struct Packet {
	
	char type;
	char ip[16];
	uint16_t port;

	char data_type;
	char topic[51];
	int sf;

	char buff[1501];
} Packet;

// folosita in server.c si stocheaza datele despre un client (tcp sau udp)
typedef struct client{
	char id[10];
	int socket;

	int topics_len;
	struct topic topics[100];

	int unsent_len;
	struct tcp_struct unsent[100];
	int online;
} client;

#endif