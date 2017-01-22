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
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <thread>
#include <vector>

#define THRNUM 4

using namespace std;

char readbuf[1000];

int port;


void read_func(int fd){
	cout << "starting read thread" << endl;
	while(true){
		int ret = read(fd, readbuf, sizeof(readbuf));
		if(ret < 0){
			cout << strerror(errno) << endl;
			if(errno != EINTR || errno != EAGAIN) break;
		}
		else if(ret == 0){
			close(fd);
			cout << "read EOF" << endl;
			break;
		}
		else{
			cout << "received bytes:" << ret << endl;
			cout << string(readbuf, ret) << endl;
		}

	}
	cout << "exiting read thread" << endl;
}

string gen_str(){
	char c = 'A' + rand()%26;
	int len = 4 + rand()%16;
	return string(len, c);
}
void write_func(string ip){
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	if(inet_pton(AF_INET, ip.c_str(), &servaddr.sin_addr) <= 0 ){
		cout << "inet_pton() failed" << endl;
		exit(-1);
	}

	if(connect(sockfd, (struct sockaddr *)& servaddr, sizeof(servaddr)) < 0){
		cout << "connect failed" << endl;
		exit(-1);
	}

	thread reader = thread(read_func, sockfd);

	while(true){
		string line = gen_str();
		cout << "writing " << line << endl;
		int nw = 0;
		while(true){
			cout << " writing " << nw << endl;
			int ret = write(sockfd, line.c_str()+nw, line.size() - nw);
			if(ret<0){
				if(errno != EINTR || errno != EAGAIN) break;
			}
			if(ret == 0) break;
			nw += ret;
		}
		sleep(1);

	}
	reader.join();
	close(sockfd);
}

int main(int argc, char** argv){
	port = atoi(argv[2]);
	vector<thread> writer;
	for(int i = 0; i < THRNUM; ++i) {
		writer.push_back(thread(write_func, argv[1]));
	}
	for(int i = 0; i < THRNUM; ++i) {
		writer[i].join();
	}
	
}


