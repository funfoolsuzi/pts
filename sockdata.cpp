#include "sockdata.hpp"

char sock_recvchar(int sockfd){
	char x;
	if(recv(sockfd, &x, sizeof x, 0)==-1){
		if(errno!=EWOULDBLOCK)std::cout<<"sock_recvchar:"<<strerror(errno)<<std::endl;
		throw no_data_in_sock;
	}
	return x;
}

char sock_peekchar(int sockfd){
	char x;
	if(recv(sockfd, &x, sizeof x, MSG_PEEK)==-1){
		if(errno!=EWOULDBLOCK)std::cout<<"sock_peekchar:"<<strerror(errno)<<std::endl;
		throw no_data_in_sock;
	}
	return x;
}
