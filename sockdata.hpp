#ifndef _SOCK_DATA_HPP

#define _SOCK_DATA_HPP

#include <iostream>
#include <cerrno>
#include <cstring>
#include <sys/socket.h>


const int no_data_in_sock = 100;

char sock_recvchar(int sockfd);
char sock_peekchar(int sockfd);

#endif
