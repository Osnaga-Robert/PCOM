/*
 * Protocoale de comunicatii
 * Laborator 7 - TCP si mulplixare
 * client.c
 */

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/tcp.h>

#include "common.h"
#include "helpers.h"

void run_client(int sockfd, char *id, struct sockaddr_in serv_addr)
{
  int rc;
  char buf[MSG_MAXSIZE + 1];
  //memset(buf, 0, MSG_MAXSIZE + 1);
  struct pollfd pfds[MAX_CONNECTIONS];
  int nfds = 0;

  struct chat_packet sent_packet;
  struct chat_packet recv_packet;

  //cei doi socketi pe care o sa-i folosim sune cei de input de la tastaura si cand primim mesaje

  pfds[nfds].fd = STDIN_FILENO;
  pfds[nfds].events = POLLIN;
  nfds++;

  pfds[nfds].fd = sockfd;
  pfds[nfds].events = POLLIN;
  nfds++;

  strcpy(sent_packet.message,id);

  rc = send_all(sockfd, &sent_packet, sizeof(sent_packet));
  DIE(rc < 0,"send");

  while (1)
  { /* server loop */
    /* wait for readiness notification */
    poll(pfds, nfds, -1);

    //atunci cand primim un mesaj de la tastatura
    if ((pfds[0].revents & POLLIN))
    {
      char *a = fgets(buf, sizeof(buf), stdin);
      strcpy(sent_packet.message, buf);
      //trimitem mesajul (adica ce am primit de la tastatura)
      rc = send_all(sockfd, &sent_packet, sizeof(struct chat_packet));
      DIE(rc < 0,"send");
      fflush(stdin);
    }
    else if ((pfds[1].revents & POLLIN))
    {
      //daca am primit ceva pe socket-ul nostru, receptionam mesajul
      int rc = recv_all(sockfd, &recv_packet, sizeof(recv_packet));
      DIE(rc < 0,"recv");
      if (rc <= 0)
      {
        break;
      };
    }
    if ((pfds[1].revents & POLLIN))
    {
      //mesajele aferente in functie de ce primim de la server
      char ip[20];
      inet_ntop(AF_INET, &(serv_addr.sin_addr), ip, INET_ADDRSTRLEN);
      //case in functie de info-ul lui recv
      switch(recv_packet.info){
        case 1:{
          printf("%s:%d - %s - %s - %s\n",recv_packet.ip, recv_packet.port,recv_packet.topic, recv_packet.type, recv_packet.message);
          break;
        }
        case 2:{
          return;
        }
        case 3:{
          printf("Already subscribed.\n");
          break;
        }
        case 4:{
          printf("Subscribed to topic.\n");
          break;
        }
        case 5:{
          printf("Unsubscribed from topic.\n");
          break;
        }
        case 6:{
          printf("Is not subscribed to this topic.\n");
          break;
        }
        case 8:{
          return;
        }
        default:{
          printf("Possible commands: exit subscribe unsubscribe\n");
          break;
        }
      }
      fflush(stdout);
      fflush(stdin);
    }
    if ((pfds[0].revents & POLLIN))
    {
      //daca primim comanda exit de la tastatura atunci clientul se va opri
      if ((strcmp(sent_packet.message, "exit\n")) == 0 || (strcmp(sent_packet.message, "exit") == 0))
      {
        return;
      }
    }
  }
}

int main(int argc, char *argv[])
{
  setvbuf(stdout, NULL, _IONBF, BUFSIZ);
  int sockfd = -1;

  if (argc != 4)
  {
    printf("\n Usage: %s <ID_CLIENT> <ip> <port>\n", argv[0]);
    return 1;
  }

  // Parsam port-ul ca un numar
  uint16_t port;
  int rc = sscanf(argv[3], "%hu", &port);
  DIE(rc != 1, "Given port is invalid");
  int flag = 1;

  // Obtinem un socket TCP pentru conectarea la server
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  DIE(sockfd < 0, "socket");

  // Completăm in serv_addr adresa serverului, familia de adrese si portul
  // pentru conectare
  struct sockaddr_in serv_addr;
  socklen_t socket_len = sizeof(struct sockaddr_in);

  memset(&serv_addr, 0, socket_len);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);
  rc = inet_pton(AF_INET, argv[2], &serv_addr.sin_addr.s_addr);
  DIE(rc <= 0, "inet_pton");

  // Ne conectăm la server
  rc = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
  setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
  DIE(rc < 0, "connect");

  run_client(sockfd, argv[1], serv_addr);

  // Inchidem conexiunea si socketul creat
  close(sockfd);

  return 0;
}