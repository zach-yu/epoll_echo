/*
 * echo_cli.cpp
 *
 *  Created on: 2015年6月25日
 *      Author: yuze
 */
#include <string>
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <iostream>
#include <fcntl.h>
#include <sys/epoll.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <thread>

using namespace std;

char readbuf[1000];

void read_func(int fd){
	while(true){
		int ret = read(fd, readbuf, sizeof(readbuf));
		if(ret < 0){
			cout << strerror(errno) << endl;
			continue;
		}
		else if(ret == 0){
			close(fd);
			break;
		}
		else{
			cout << "received bytes:" << ret << endl;
		}

	}
}

int main(int argc, char** argv){
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi(argv[2]));
	if(inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0 ){
		cout << "inet_pton() failed" << endl;
		exit(-1);
	}

	if(connect(sockfd, (struct sockaddr *)& servaddr, sizeof(servaddr)) < 0){
		cout << "connect failed" << endl;
		exit(-1);
	}

	thread read_th(read_func, sockfd);

	string line;
	while(getline(std::cin, line)){
		int ret = write(sockfd, line.c_str(), line.size());
		if(ret != (int)line.size()){
			cout << "write error" << endl;
			break;
		}

	}
	shutdown(sockfd, SHUT_WR);
	read_th.join();
}


