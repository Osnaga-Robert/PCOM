/*
 * Protocoale de comunicatii
 * Laborator 7 - TCP
 * Echo Server
 * server.c
 */

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>
#include <netinet/tcp.h>

#include "common.h"
#include "helpers.h"

#define MAX_CONNECTIONS 32

// functie prin care decodificam mesajul in functie de ce mesaj primim

void decode_message(struct udp_packet *received, struct chat_packet *sent_packet, struct sockaddr_in udp_addr)
{
	char port[20];
	(*sent_packet).info = 1;
	strcpy((*sent_packet).topic, received->topic);

	memset(port, 0, 20);
	// tipul de date INT
	if (received->tip_date == 0)
	{
		strcpy((*sent_packet).type, "INT");

		long number = ntohl(*(uint32_t *)(received->continut + 1));
		if (received->continut[0] != 0)
		{
			number *= -1;
		}
		sprintf(port, "%ld", number);
		strcpy((*sent_packet).message, port);
	}
	// tipul de date SHORT_REAL
	else if (received->tip_date == 1)
	{
		strcpy((*sent_packet).type, "SHORT_REAL");
		double number = ntohs(*(uint16_t *)(received->continut));
		number /= 100;
		sprintf(port, "%.2f", number);

		strcpy((*sent_packet).message, port);
	}
	// tipul de date FLOAT
	else if (received->tip_date == 2)
	{
		strcpy((*sent_packet).type, "FLOAT");
		double number = ntohl(*(uint32_t *)(received->continut + 1));
		number = number / pow(10, received->continut[5]);

		if (received->continut[0] != 0)
		{
			number *= -1;
		}
		sprintf(port, "%f", number);

		strcpy((*sent_packet).message, port);
	}
	// tipul de date STRING
	else if (received->tip_date == 3)
	{
		strcpy((*sent_packet).type, "STRING");
		strcpy((*sent_packet).message, received->continut);
	}
}

int run_chat_multi_server(int listenfd_tcp, int listenfd_udp, struct sockaddr_in udp_addr, socklen_t socket_len_udp, char *port)
{
	// vom avea la inceput trei socket, unul de pe care asculta conexiuni TCP, unul pentru ascultarea mesajelor UDP si unul pentru
	// a receptiona mesaje de la clientii TCP
	struct pollfd poll_fds[MAX_CONNECTIONS];
	int num_clients = 3;
	int rc;
	int flag = 1;

	struct chat_packet received_packet;
	struct chat_packet sent_packet;

	// Setam socket-ul listenfd_tcp pentru ascultare
	rc = listen(listenfd_tcp, MAX_CONNECTIONS);
	DIE(rc < 0, "listen");

	// se adauga noul file descriptor (socketul pe care se asculta conexiuni) in
	// multimea read_fds
	poll_fds[0].fd = listenfd_tcp;
	poll_fds[0].events = POLLIN;
	poll_fds[1].fd = listenfd_udp;
	poll_fds[1].events = POLLIN;
	poll_fds[2].fd = STDIN_FILENO;
	poll_fds[2].events = POLLIN;
	char buffer[11];
	char buffer_message[MSG_MAXSIZE];
	char sent_message_udp[MSG_MAXSIZE];

	struct tcp_server_id clients_on[5];
	struct tcp_server_id clients_off[5];
	int clients_on_length = 0;
	int clients_off_length = 0;

	socklen_t cli_len;
	int newsockfd;
	struct sockaddr_in cli_addr;
	int first_message = 0;

	while (1)
	{

		rc = poll(poll_fds, num_clients, -1);
		DIE(rc < 0, "poll");

		for (int i = 0; i < num_clients; i++)
		{

			if (poll_fds[i].revents & POLLIN)
			{
				if (poll_fds[i].fd == listenfd_tcp)
				{
					// a venit o cerere de conexiune pe socketul inactiv (cel cu listen),
					// pe care serverul o accepta

					cli_len = sizeof(cli_addr);
					newsockfd = accept(listenfd_tcp, (struct sockaddr *)&cli_addr, &cli_len);
					DIE(newsockfd < 0, "accept");

					memset(buffer, 0, sizeof(buffer));

					setsockopt(newsockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));

					// adaugam noul client in poll_fds

					poll_fds[num_clients].fd = newsockfd;
					poll_fds[num_clients].events = POLLIN;
					num_clients++;

					first_message = 1;
				}
				else if (poll_fds[i].fd == listenfd_udp)
				{
					// daca primim un mesaj de la un client UDP atunci vedem daca trebuie sa il salvam
					// pentru unii clienti sau daca trebuie sa il trimitem (in functie de sf-ul fiecarui client
					// si daca clientii respectivi sunt conectati sau nu)
					struct sockaddr_in client_addr;
					rc = recvfrom(listenfd_udp, buffer_message, sizeof(buffer_message), 0, (struct sockaddr *)&udp_addr, &socket_len_udp);
					DIE(rc < 0, "recvfrom");


					struct udp_packet *udp_message = (struct udp_packet *)(buffer_message);

					memset(&sent_packet, 0, sizeof(sent_packet));

					sent_packet.port = ntohs(udp_addr.sin_port);
					inet_ntop(AF_INET, &(udp_addr.sin_addr), sent_packet.ip, INET_ADDRSTRLEN);


					decode_message(udp_message, &sent_packet, client_addr);

					for (int j = 0; j < clients_off_length; j++)
					{
						for (int k = 0; k < clients_off[j].topic_len; k++)
						{
							if (strcmp(udp_message->topic, clients_off[j].topic[k].topic) == 0 && clients_off[j].sf[k] == 1)
							{
								clients_off[j].saved_messages[clients_off[j].saved_messages_len] = sent_packet;
								clients_off[j].saved_messages_len++;
							}
						}
					}

					for (int j = 0; j < clients_on_length; j++)
					{
						for (int k = 0; k < clients_on[j].topic_len; k++)
						{
							if (strcmp(udp_message->topic, clients_on[j].topic[k].topic) == 0)
							{
								sent_packet.info = 1;
								rc = send_all(clients_on[j].fd, &sent_packet, sizeof(sent_packet));
								DIE(rc < 0, "send");
							}
						}
					}

					memset(buffer_message, 0, sizeof(buffer_message));
					memset(sent_message_udp, 0, strlen(sent_message_udp) + 1);
					memset(&sent_packet, 0, sizeof(sent_packet));
				}
				else if ((poll_fds[i].fd == STDIN_FILENO))
				{
					// in cazul in care primim un mesaj de de la STDIN-un server-ului
					// comanda disponibila este doar exit
					memset(buffer, 0, sizeof(buffer));
					char *test = fgets(buffer, sizeof(buffer), stdin);
					if (strcmp(buffer, "exit\n") == 0)
					{
						memset(&sent_packet, 0, sizeof(sent_packet));
						sent_packet.info = 2;
						for (int j = 0; j < clients_on_length; j++)
						{
							rc = send_all(clients_on[j].fd, &sent_packet, sizeof(sent_packet));
							DIE(rc < 0, "send");
						}
						return 0;
					}
					else
					{
						printf("Comanda disponibila: exit\n");
					}
				}
				else
				{
					// s-au primit date pe unul din socketii de client,
					// asa ca serverul trebuie sa le receptioneze
					int rc = recv_all(poll_fds[i].fd, &received_packet,
									  sizeof(received_packet));
					DIE(rc < 0, "recv");

					if (rc == 0)
					{
						// conexiunea s-a inchis
						printf("Client %d disconnected.\n", i);
						close(poll_fds[i].fd);

						// se scoate din multimea de citire socketul inchis
						for (int j = i; j < num_clients - 1; j++)
						{
							poll_fds[j] = poll_fds[j + 1];
						}

						num_clients--;
					}
					else
					{
						int check = 0;

						// daca nu primim exit, atunci fie am primit ceva random, fie subscribe sau
						// unsubscribe, fie un ID pentru client

						if (strcmp(received_packet.message, "exit\n") != 0)
						{
							char *command = strtok(received_packet.message, " ");
							char *topic = strtok(NULL, " ");
							// in caz de subscribe adaugam clientului aferent topicul
							if (strcmp(command, "subscribe") == 0)
							{
								for (int j = 0; j < clients_on_length; j++)
								{
									if (clients_on[j].fd == poll_fds[i].fd)
									{
										int ok = 0;
										for (int k = 0; k < clients_on[j].topic_len; k++)
										{
											if (strcmp(topic, clients_on[j].topic[clients_on[j].topic_len - 1].topic) == 0)
											{
												sent_packet.info = 3;
												rc = send_all(clients_on[j].fd, &sent_packet, sizeof(sent_packet));
												ok = 1;
											}
										}
										if (ok == 0)
										{
											strcpy(clients_on[j].topic[clients_on[j].topic_len].topic, topic);
											char *sf = strtok(NULL, " ");
											if (strcmp(sf, "0\n") == 0)
											{
												clients_on[j].sf[clients_on[j].topic_len] = 0;
											}
											else
											{
												clients_on[j].sf[clients_on[j].topic_len] = 1;
											}
											sent_packet.info = 4;
											clients_on[j].topic_len++;
											rc = send_all(clients_on[j].fd, &sent_packet, sizeof(sent_packet));
											DIE(rc < 0, "send");
											memset(&sent_packet, 0, sizeof(struct chat_packet));
										}
									}
								}
							}

							// daca este unsubscribe dezabonam clientul de la topic-ul respectiv
							else if (strcmp(command, "unsubscribe") == 0)
							{
								for (int j = 0; j < clients_on_length; j++)
								{
									if (clients_on[j].fd == poll_fds[i].fd)
									{
										if (clients_on[j].topic_len == 0)
										{
											sent_packet.info = 6;
											rc = send_all(clients_on[j].fd, &sent_packet, sizeof(sent_packet));
											DIE(rc < 0, "send");
										}
										else
										{
											for (int k = 0; k < clients_on[j].topic_len; k++)
											{
												if (strtok(topic, clients_on[j].topic[k].topic) != NULL)
												{
													for (int l = k; l < clients_on[k].topic_len - 1; l++)
													{
														clients_on[j].topic[k] = clients_on[j].topic[k + 1];
														clients_on[j].sf[k] = clients_on[j].sf[k + 1];
													}
													sent_packet.info = 5;
													clients_on[j].topic_len--;
													rc = send_all(clients_on[j].fd, &sent_packet, sizeof(sent_packet));
													DIE(rc < 0, "send");
												}
												else
												{
													sent_packet.info = 6;
													rc = send_all(clients_on[j].fd, &sent_packet, sizeof(sent_packet));
													DIE(rc < 0, "send");
												}
											}
										}
									}
								}
							}
							else
							{
								// daca primim un ID deja conectat vom scoate deconecta "clientul" cu acelasi ID
								for (int i = 0; i < clients_on_length; i++)
								{
									if (strcmp(clients_on[i].id, received_packet.message) == 0)
									{
										printf("Client %s already connected.\n", received_packet.message);
										strcpy(sent_packet.message, "exit");
										sent_packet.info = 8;
										rc = send_all(poll_fds[num_clients - 1].fd, &sent_packet, sizeof(sent_packet));
										DIE(rc < 0, "send");
										close(poll_fds[num_clients - 1].fd);
										check = 1;
										memset(&sent_packet, 0, sizeof(struct chat_packet));
										num_clients--;
										break;
									}
								}

								if (check == 0)
								{

									if (first_message == 1)
									{
										// in cazul in care se conecteaza un client nou, il adaugam in structurile noastre
										// sau il adaugam din structura care stocheza clientii care au fost on, iar pana in
										// acest moment erau off
										first_message = 0;
										printf("New client %s connected from %s:%s.\n",
											   received_packet.message, inet_ntoa(cli_addr.sin_addr), port);

										for (int j = 0; j < clients_off_length; j++)
										{
											if (strcmp(clients_off[j].id, received_packet.message) == 0)
											{
												if (clients_off[j].saved_messages_len != 0)
												{
													memset(&sent_packet, 0, sizeof(struct chat_packet));
													for (int k = 0; k < clients_off[j].saved_messages_len; k++)
													{
														sent_packet.info = 1;
														sent_packet = clients_off[j].saved_messages[k];
														rc = send_all(clients_off[j].fd, &sent_packet, sizeof(sent_packet));
														DIE(rc < 0, "send");
													}
													memset(clients_off[j].saved_messages, 0, sizeof(clients_off[j].saved_messages));
												}
												clients_on[clients_on_length] = clients_off[j];
												clients_on[clients_on_length].fd = poll_fds[num_clients - 1].fd;
												clients_on_length++;
												check = 1;
												for (int k = j; k < clients_off_length - 1; k++)
												{
													clients_off[k] = clients_off[k + 1];
												}
												clients_off_length--;
											}
										}

										if (check == 0)
										{
											strcpy(clients_on[clients_on_length].id, received_packet.message);
											clients_on[clients_on_length].topic_len = 0;
											clients_on[clients_on_length].saved_messages_len = 0;
											clients_on[clients_on_length].fd = poll_fds[num_clients - 1].fd;
											memset(clients_on[clients_on_length].saved_messages, 0, sizeof(clients_on[clients_on_length].saved_messages));
											memset(clients_on[clients_on_length].sf, 0, sizeof(clients_on[clients_on_length].sf));
											memset(clients_on[clients_on_length].topic, 0, sizeof(clients_on[clients_on_length].topic));
											clients_on_length++;
										}

										memset(received_packet.message, 0, sizeof(received_packet.message));
									}
									else
									{
										sent_packet.info = 7;
										// in cazul in care nu primim niciuna dintre comenzi
										rc = send_all(poll_fds[num_clients - 1].fd, &sent_packet, sizeof(sent_packet));
										DIE(rc < 0, "send");
									}
								}
							}
						}
						else
						{
							// altel primim exit iar serverul trebuie sa se inchida alaturi de toti clinetii care sunt on
							printf("Client %s disconnected.\n", clients_on[i - 3].id);
							close(poll_fds[i].fd);

							clients_off[clients_off_length] = clients_on[i - 3];
							clients_off_length++;

							// se scoate din multimea de citire socketul inchis
							for (int j = i; j < num_clients - 1; j++)
							{
								poll_fds[j] = poll_fds[j + 1];
							}
							for (int j = i - 3; j < clients_on_length - 1; j++)
							{
								clients_on[j] = clients_on[j + 1];
							}
							clients_on_length--;
							num_clients--;
						}
					}
				}
			}
		}
		fflush(stdout);
	}
}

int main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	if (argc != 2)
	{
		printf("\n Usage: %s <port>\n", argv[0]);
		return 1;
	}

	// Parsam port-ul ca un numar
	uint16_t port;
	int rc = sscanf(argv[1], "%hu", &port);
	DIE(rc != 1, "Given port is invalid");

	// Obtinem un socket TCP si UDP pentru receptionarea conexiunilor
	int listenfd_tcp = socket(AF_INET, SOCK_STREAM, 0);
	DIE(listenfd_tcp < 0, "socket");
	int listenfd_udp = socket(AF_INET, SOCK_DGRAM, 0);
	DIE(listenfd_udp < 0, "socket");

	// Completăm in tcp_addr adresa serverului, familia de adrese si portul
	// pentru conectare
	struct sockaddr_in tcp_addr;
	struct sockaddr_in udp_addr;
	socklen_t socket_len_tcp = sizeof(struct sockaddr_in);
	socklen_t socket_len_udp = sizeof(struct sockaddr_in);

	// Facem adresa socket-ului reutilizabila, ca sa nu primim eroare in caz ca
	// rulam de 2 ori rapid
	int enable1 = 1;
	int enable2 = 1;
	if (setsockopt(listenfd_tcp, SOL_SOCKET, SO_REUSEADDR, &enable1, sizeof(int)) < 0)
		perror("setsockopt(SO_REUSEADDR) failed");
	if (setsockopt(listenfd_udp, SOL_SOCKET, SO_REUSEADDR, &enable2, sizeof(int)) < 0)
		perror("setsockopt(SO_REUSEADDR) failed");

	memset(&tcp_addr, 0, socket_len_tcp);
	tcp_addr.sin_family = AF_INET;
	tcp_addr.sin_port = htons(port);

	memset(&udp_addr, 0, socket_len_udp);
	udp_addr.sin_family = AF_INET;
	udp_addr.sin_port = htons(port);

	// Asociem adresa serverului cu socketul creat folosind bind
	rc = bind(listenfd_tcp, (const struct sockaddr *)&tcp_addr, sizeof(tcp_addr));
	DIE(rc < 0, "bind");
	rc = bind(listenfd_udp, (const struct sockaddr *)&udp_addr, sizeof(udp_addr));
	DIE(rc < 0, "bind");

	run_chat_multi_server(listenfd_tcp, listenfd_udp, udp_addr, socket_len_udp, argv[1]);

	// Inchidem listenfd_tcp
	close(listenfd_tcp);

	return 0;
}