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

#include "Connection.h"
#include "ExecutorService.h"

using namespace std;

const int MAX_EVENTS = 10000;

ExecutorService<void, packaged_task<void()>> _executor_service;

void set_nonblocking(int fd){
	int val = fcntl(fd, F_GETFL, 0);
	if(val < 0){
		cout << "fcntl() error: " << errno<< endl;
		_exit(1);
	}
	val = fcntl(fd, F_SETFL, val | O_NONBLOCK);
	if(val < 0){
		cout << "fcntl() set error: " << strerror(errno) << endl;
		_exit(1);
	}

}


void set_socket_buffer(int fd, int size, int opt){
	int n;
	unsigned int size_of_n = sizeof(n);
	int get_opt = opt;

	if( getsockopt(fd, SOL_SOCKET, get_opt, (void *)&n, &size_of_n) < 0 ){
		cout << "getsockopt SO_SNDBUF failed:" << strerror(errno) << endl;
	}
	else {
		cout << "send buffer size:" << n << endl;
	}


	if( setsockopt(fd,SOL_SOCKET,opt,(void *)&size, sizeof(size)) < 0 ){
		cout << "setsockopt SO_SNDBUF failed:" << strerror(errno) << endl;
	}

	if( getsockopt(fd, SOL_SOCKET, get_opt, (void *)&n, &size_of_n) < 0 ){
		cout << "getsockopt SO_SNDBUF failed:" << strerror(errno) << endl;
	}
	else {
		cout << "send buffer size after set:" << n << endl;
	}
}

void bind_socket_listen(int fd,  int port){
	const int on = 1;
	if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof (on) ) < 0){
		cout << "bind() error: " << strerror(errno) << endl;;
	}

	struct sockaddr_in addr;
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);
	int ret = bind(fd, (struct sockaddr *)&addr, sizeof(addr));
	if(ret < 0){
		cout << "bind() error: " << errno << endl;
		_exit(1);
	}

	ret = listen(fd, 10000);
	if(ret < 0){
		cout << "listen() error: " << errno << endl;
		_exit(1);
	}
}

// write until blocked, or until n chars have been written
int writen(int fd, char *buf, int n){
	int nwrite = 0;
	while(nwrite < n){
		int ret = write(fd, buf + nwrite, n - nwrite);
		if(ret < 0){
			if(errno == EINTR){
				continue;
			}
			else{
				break;
			}
		}
		else{
			nwrite += ret;

		}
	}
	return nwrite;

}

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

void do_use_fd(int epollfd, struct epoll_event* event){

	Connection* conn = (Connection*)event->data.ptr;
	cout << "conn:" << conn << endl;
	int fd = conn->_conn_fd;

	cout << "event " << event->events << " on conn " << conn <<", fd=" << fd << endl;
	if(event->events & EPOLLOUT){
		cout << "flushing write buffer queue" << endl;
		conn->flushWBuffer();
	}

	cout << "read connection " << conn << endl;
	while(true){
		// new message
		if(conn->_state == Connection::READY){
			cout << "state is Connection::READY" << endl;
			ByteBuffer headbuf (4);
			cout << " reading header" << endl;
			conn->readMessageHeader(headbuf);

			// save the buffer for next read and task
			auto header_buf_ptr = make_shared<ByteBuffer>(move(headbuf));
			conn->setReadBuffer(header_buf_ptr);

			if(header_buf_ptr->remaining() > 0){
				if(conn->_state != Connection::ERROR && conn->_state != Connection::CLOSED){
					// blocked
					cout << "blocked on reading header" << endl;

				}
				break;
			}
			else{
				cout << "finished header, submit header task" << endl;
				packaged_task<void()> task(HeaderTask(*(conn->getReadBuffer())));
				_executor_service.submit(task);
				//conn->processHeader(headbuf);
			}
		}

		else if(conn->_state == Connection::READING_HEADER){
			cout << "state is Connection::READING_HEADER" << endl;
			conn->readRemaining();
			if(conn->rbufRemaining() > 0){
				// blocked or error
				break;
			}
			else{
				// process header
				//conn->processHeader();
				cout << "finished header, submit header task2" << endl;
				packaged_task<void()> task(HeaderTask(*(conn->getReadBuffer())));
				_executor_service.submit(task);
			}
		}

		else if(conn->_state == Connection::FINISHED_HEADER){
			cout << "state is Connection::FINISHED_HEADER" << endl;
			// TODO: get length from header
			//ByteBuffer bodybuf (_body_len);
			ByteBuffer bodybuf (16);
			conn->readMessageBody(bodybuf);
			cout << "remaining:" << bodybuf.remaining() << endl;

			// save the buffer for next read
			auto body_buf_ptr = make_shared<ByteBuffer>(move(bodybuf));
			conn->setReadBuffer(body_buf_ptr);

			if(body_buf_ptr->remaining() > 0){
				if(conn->_state != Connection::ERROR && conn->_state != Connection::CLOSED){
					cout << "blocked on reading body" << endl;
				}
				break;
			}
			else{
				//process body
				//conn->processBody();
				cout << "finished body, submit body task" << endl;
				packaged_task<void()> task(BodyTask(conn, *(conn->getReadBuffer())));
				_executor_service.submit(task);
			}
		}

		else if(conn->_state == Connection::READING_BODY){
			cout << "state is Connection::READING_BODY" << endl;
			conn->readRemaining();
			if(conn->rbufRemaining() > 0){
				// blocked
				break;
			}
			// finished reading body
			else{
				//process body
				//conn->processBody();
				cout << "finished body, submit body task2" << endl;
				packaged_task<void()> task(BodyTask(conn, *(conn->getReadBuffer())));
				_executor_service.submit(task);
				//_executor_service.submit(move(HeaderTask(this)));

			}
		}
	}
	if(conn->_state == Connection::CLOSED || conn->_state == Connection::ERROR){
		cout << "state is Connection::CLOSED||Connection::ERROR" << endl;
		delete conn;
	}
}

int main(int argc, char **argv){

	ConnMap conn_map;
	int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(listen_fd < 0){
		cout << "socket() error: " << errno;
		_exit(1);
	}

	set_nonblocking(listen_fd);

	bind_socket_listen(listen_fd, atoi(argv[1]));

	struct epoll_event ev, events[MAX_EVENTS];
    int epollfd = epoll_create(1);
    if (epollfd == -1) {
        perror("epoll_create");
        _exit(-1);
    }

    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_fd, &ev) == -1) {
        perror("epoll_ctl: listen_sock");
        _exit(-1);
    }

    int nfds;
    for (;;) {
    	cout << "epoll_wait" << endl;
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
        	cout << "epoll_wait failed" << endl;
            perror("epoll_wait");
            _exit(-1);
        }
        cout << "epoll_wait woke up" << endl;
        struct sockaddr_in local;
        socklen_t addrlen = sizeof(local);
        int conn_fd;
        for (int n = 0; n < nfds; ++n) {
            if (events[n].data.fd == listen_fd) {
                conn_fd = accept4(listen_fd, (struct sockaddr *) &local, &addrlen, SOCK_NONBLOCK);
                if (conn_fd == -1) {
                	if(errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR || errno == ECONNABORTED){
                		continue;
                	}
                    perror("accept");
                    cout << "accept() error: " << errno;
                    _exit(-1);
                }
                //set_nonblocking(conn_fd);
                // set send buffer to the lowest limit, for testing purpose
                set_socket_buffer(conn_fd, 2, SO_SNDBUF);


                Connection* new_conn = new Connection(conn_fd, epollfd);
                cout << "add new conn " << new_conn <<", fd=" << conn_fd << endl;
                conn_map.insert(make_pair(conn_fd, new_conn));
                ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
                ev.data.ptr = new_conn;
                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_fd, &ev) == -1) {
                    perror("epoll_ctl: conn_sock");
                    _exit(-1);
                }
            } else {
                do_use_fd(epollfd, &events[n]);
            }
        }
    }







}
