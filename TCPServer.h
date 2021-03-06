/*
 * TCPServer.h
 *
 *  Created on: 2015年7月3日
 *      Author: yuze
 */

#ifndef TCPSERVER_H_
#define TCPSERVER_H_

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
#include <signal.h>

#include "ExecutorService.h"
#include "Connection.h"

class HeaderTask{
	ByteBuffer _rbuf;
public:
	HeaderTask(const ByteBuffer& buffer) : _rbuf(buffer){}

	void operator()() const{
		cout << "worker::received header:" << string((const char *)_rbuf.getBuffer(), _rbuf.getLimit()) << endl;
	}
};

class BodyTask{
	Connection* _conn;
	ByteBuffer _rbuf;
public:
	BodyTask(Connection* conn, const ByteBuffer& buffer) : _conn(conn), _rbuf(buffer){}

	void operator()() const{
		cout << "worker::received body:" << string((const char *)_rbuf.getBuffer(), _rbuf.getLimit()) << endl;
		// decode body and call processor
		int to_write = 0;
		// for testing blocking write
		unsigned char *WBUF = new unsigned char[12800];
		size_t nread = _rbuf.getLimit();
		for( unsigned char* p = WBUF; p + nread < WBUF + 12800; p+= nread){
			memcpy(p, _rbuf.getBuffer(), nread);
			to_write += nread;
		}
		*(WBUF + to_write - 1) = 'X' ;
		//
		auto wbuf = make_shared<ByteBuffer>(ByteBuffer());
		wbuf->setBuffer(WBUF, to_write);
		_conn->addWBuffer(wbuf);
		_conn->register_event(EPOLLIN | EPOLLOUT);
	}
};



class TCPServer {
public:
	TCPServer(int port) : _port(port), _executor_service(4){
		_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
		if(_listen_fd < 0){
			cout << "socket() error: " << errno;
			_exit(1);
		}
		signal(SIGINT, sig_handler);
		signal(SIGTERM, sig_handler);
		signal(SIGPIPE, SIG_IGN);

		set_nonblocking(_listen_fd);

		bind_socket_listen(_listen_fd);

		_epoll_fd = epoll_create(256);
		if (_epoll_fd == -1) {
			perror("epoll_create");
			_exit(-1);
		}

		struct epoll_event ev;
		ev.events = EPOLLIN | EPOLLONESHOT | EPOLLET;
		ev.data.fd = _listen_fd;
		cout << "add " << ev.data.fd << " for listen event " << endl;
		if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _listen_fd, &ev) == -1) {
			perror("epoll_ctl: listen_sock");
			_exit(-1);
		}
		pipe(pp);
		//ev.events = EPOLLIN | EPOLLONESHOT | EPOLLET;
		ev.events = EPOLLIN | EPOLLET;
		if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, pp[0], &ev) == -1) {
			perror("epoll_ctl: listen_sock");
			_exit(-1);
		}

	}

	void dowork();
	virtual ~TCPServer() {}

	static void *thread_helper(void *arg){
		cout << "starting thread " << pthread_self() << endl;
		TCPServer *me = static_cast<TCPServer *>(arg);
		me->dowork();
		return NULL;
	}

	void start(){
		for(int i = 0; i < 4; ++i){

			int r = pthread_create(&thread[i], NULL,&TCPServer::thread_helper, this);
			if(r < 0){
				perror("pthread_create");
				_exit(-1);
			}
		}
		for(int i = 0; i < _thcount; ++i) pthread_join(thread[i], NULL);
	}

private:

	int _port;
	int _listen_fd, _epoll_fd;
	static const int _thcount = 4;
	pthread_t thread[100];
	static const int MAX_EVENTS = 1000;
	ExecutorService<void, packaged_task<void()>> _executor_service;
	static bool _exiting;
        int pp[2];

	void set_nonblocking(int fd);

	void bind_socket_listen(int fd);

	void do_use_fd(struct epoll_event* event);
	void do_use_fd1(struct epoll_event* event);

	void set_socket_buffer(int fd, int size, int opt);

	void install_fd(int fd, int events, void *data);
	void reinstall_fd(int fd, int events);
	void reinstall_fd(int fd, int events, void *data);
	void remove_fd(int fd);

	static void sig_handler(int signo){
		cout << "caught signal:" << signo << endl;
		if(signo == SIGINT || signo == SIGTERM){
			//_exiting = true;
			exit(1);
			//write(pp[1], "X", 1);
		}
	}
};



#endif /* TCPSERVER_H_ */
