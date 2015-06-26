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



using namespace std;

const int MAX_EVENTS = 10000;

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
	/*
	if(opt == SO_SNDBUFFORCE){
		get_opt = SO_SNDBUF;
	}*/
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

// these data should be connection-wise. each connection should have these attributes!
int remain = 0;
int offset = 0;
char BUF[128];
char WBUF[12800];

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
				return nwrite;
			}
		}
		else{
			nwrite += ret;

		}
	}
	return nwrite;

}

// register ONESHOT event
int register_event(int epollfd, int fd, int events){
	struct epoll_event ev;
	// is EPOLLET needed, if we have EPOLLONESHOT?
    ev.events = events | EPOLLET | EPOLLONESHOT;
    ev.data.fd = fd;
	if (epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &ev) == -1) {
		cout << " EPOLL_CTL_MOD failed" << endl;
		return -1;
	}
	return 0;
}


void do_use_fd(int epollfd, struct epoll_event* event){
	// echo
	int nread = 0;
	int fd = event->data.fd;

	if(event->events & EPOLLOUT){
		cout << "write remaining " << remain << endl;;
		int nwrite = writen(fd, WBUF + offset, remain);
		if(nwrite == remain){
			cout << "finished, nwrite:" << nwrite << ", last:" << WBUF[offset + nwrite - 1] << endl;
			offset = 0;
			remain = 0;
			// remove write event notification
			cout << "remove write event" << endl;
			register_event(epollfd, fd, EPOLLIN);
		}
		else{
			// block again, or other error occurs
			offset += nwrite;
			remain -= nwrite;
			cout << strerror(errno) <<", nwrite:" << nwrite << ", remain:" << remain << endl;
			//cout << "register EPOLLOUT" << endl;
			//register_event(epollfd, fd,  EPOLLIN | EPOLLOUT);

		}


	}
	while(true){
		nread = read(event->data.fd, BUF, sizeof(BUF));
		if(nread < 0){
			// handle read error
			if(errno == EINTR){
				continue;
			}
			else if(errno == EWOULDBLOCK || errno == EAGAIN){
				cout << " would block on read" << endl;
				// reinstall event
				if(remain){
					register_event(epollfd, fd, EPOLLIN | EPOLLOUT);
				}
				else{
					register_event(epollfd, fd, EPOLLIN);

				}
			}
			else if(errno == ECONNRESET){
				cout << "conn is reset by peer" << endl;
				close(fd);
			}
			else{
				cout << "read failed with errno:" << strerror(errno) << endl;
				close(fd);

			}
			return;
		}
		// EOF, close my end.
		else if(nread == 0){
			cout << "client closing" << endl;
			close(fd);
			return;
		}
		else{
			int to_write = 0;
			// for testing blocking write
			for( char* p = WBUF; p + nread < WBUF + sizeof(WBUF); p+= nread){
				memcpy(p, BUF, nread);
				to_write += nread;
			}
			*(WBUF + to_write - 1) = 'X';
			//

			cout << "writing total : " << to_write << endl;
			int nwrite = writen(fd, WBUF, to_write);
			// error occurs
			if(nwrite < to_write){
				if(errno == EWOULDBLOCK || errno == EAGAIN){
					cout << " would block on write, nwrite=" << nwrite << ", remain=" << to_write - nwrite << endl;
					offset = nwrite;
					remain = to_write - nwrite;
					// add write event
					register_event(epollfd, fd, EPOLLIN | EPOLLOUT);
					return;
				}
				// TODO: RST has been received, SIGPIPE should be ingored!
				else if(errno == EPIPE){
					cout << "invalid pipe" << endl;
					return;
				}

			}

		}

	}

}

int main(int argc, char **argv){

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

                cout << "add new conn" << endl;
                ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
                ev.data.fd = conn_fd;
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
