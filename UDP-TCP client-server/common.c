#include "common.h"

#include <sys/socket.h>
#include <sys/types.h>

/*
    TODO 1.1: Rescrieți funcția de mai jos astfel încât ea să facă primirea
    a exact len octeți din buffer.
*/
int recv_all(int sockfd, void *buffer, size_t len) {

  size_t bytes_received = 0;
  size_t bytes_remaining = len;
  char *buff = buffer;
  

      while(bytes_remaining) {
          int bytes = recv (sockfd, &buff[bytes_received], len, 0);
          bytes_remaining -= bytes;
          bytes_received += bytes;
      }

  
  return bytes_received;
}

/*
    TODO 1.2: Rescrieți funcția de mai jos astfel încât ea să facă trimiterea
    a exact len octeți din buffer.
*/

int send_all(int sockfd, void *buffer, size_t len) {
  size_t bytes_sent = 0;
  size_t bytes_remaining = len;
  char *buff = buffer;
  
      while(bytes_remaining) {
          int bytes = send(sockfd, &buff[bytes_sent], len, 0);
          bytes_remaining -= bytes;
          bytes_sent += bytes;
      }

  
  return bytes_sent;
}