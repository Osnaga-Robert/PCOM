#ifndef __COMMON_H__
#define __COMMON_H__

#include <stddef.h>
#include <stdint.h>

int send_all(int sockfd, void *buff, size_t len);
int recv_all(int sockfd, void *buff, size_t len);

/* Dimensiunea maxima a mesajului */
#define MSG_MAXSIZE 2048
#define MAX_CONNECTIONS 32

struct __attribute__((packed)) chat_packet {
  char ip[20];
  int port;
  int info;
  char type[11];
  char message[1501];
  char topic[51];
};

struct __attribute__((packed)) udp_packet {
  char topic[50];
  uint8_t tip_date;
  char continut[1501];
};

struct __attribute__((packed)) messages{
  char message[1501];
};

struct __attribute__((packed)) topics{
  char topic[51];
};

struct __attribute__((packed)) tcp_server_id {
  int fd;
  char id[11];
  int sf[50];
  struct topics topic[50];
  int topic_len;
  struct chat_packet saved_messages[20];
  int saved_messages_len;
};




#endif
