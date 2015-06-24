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
		cout << "fcntl() set error: " << errno<< endl;
		_exit(1);
	}

}

void bind_socket_listen(int fd,  int port){
	const int on = 1;
	if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof (on) ) < 0){
		cout << "bind() error: " << errno;
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


int remain = 0;
char BUF[16];

void do_use_fd(int epollfd, struct epoll_event* event){
	// echo
	int nread = 0;
	int fd = event->data.fd;

	if(event->events | EPOLLOUT){
		write(fd, BUF, remain);
		// remove write event notification
		struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = fd;
		if (epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &ev) == -1) {
			cout << " EPOLL_CTL_MOD failed" << endl;
		}

	}
	while(true){
		nread = read(event->data.fd, BUF, sizeof(BUF));
		if(nread < 0){
			// handle read error
			if(errno == EWOULDBLOCK || errno == EAGAIN){
				cout << " would block on read" << endl;
			}
			else if(errno == ECONNRESET){
				cout << "conn is reset by peer" << endl;
			}
			else{
				cout << "read failed with errno:" << strerror(errno) << endl;

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
			int nwrite = write(fd, BUF, nread);
			if(nwrite < 0){
				if(errno == EWOULDBLOCK || errno == EAGAIN){
					remain = nread;
					struct epoll_event ev;
	                ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
	                ev.data.fd = fd;
					if (epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &ev) == -1) {
						cout << " EPOLL_CTL_MOD failed" << endl;
					}
					return;
				}
				// RST has been received, SIGPIPE should be ingored!
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
                conn_fd = accept4(listen_fd,
                                (struct sockaddr *) &local, &addrlen, SOCK_NONBLOCK);
                if (conn_fd == -1) {
                	if(errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR || errno == ECONNABORTED){
                		continue;
                	}
                    perror("accept");
                    cout << "accept() error: " << errno;
                    _exit(-1);
                }
                //set_nonblocking(conn_fd);
                cout << "add new conn" << endl;
                ev.events = EPOLLIN | EPOLLET;
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
