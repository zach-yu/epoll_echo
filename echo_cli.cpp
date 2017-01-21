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

#define THRNUM 1

using namespace std;

char readbuf[1000];

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
void write_func(int fd){
	while(true){
		string line = gen_str();
		cout << "writing " << line << endl;
		int nw = 0;
		while(true){
			cout << " writing " << nw << endl;
			int ret = write(fd, line.c_str()+nw, line.size() - nw);
			if(ret<0){
				if(errno != EINTR || errno != EAGAIN) break;
			}
			if(ret == 0) break;
			nw += ret;
		}
		sleep(1);

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

	//thread read_th(read_func, sockfd);
	vector<thread> reader;
	vector<thread> writer;
	for(int i = 0; i < THRNUM; ++i) {
		reader.push_back(thread(read_func, sockfd));
		writer.push_back(thread(write_func, sockfd));
	}
	for(int i = 0; i < THRNUM; ++i) {
		reader[i].join();
		writer[i].join();
	}
	close(sockfd);
}


