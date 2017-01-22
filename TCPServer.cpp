/*
 * TCPServer.cpp
 *
 *  Created on: 2015年7月6日
 *      Author: yuze
 */

#include "TCPServer.h"

bool  TCPServer::_exiting = false;

void TCPServer::do_use_fd1(struct epoll_event* event){
	Connection* conn = (Connection*)event->data.ptr;
	cout << "conn:" << conn << endl;
	//int fd = conn->_conn_fd;
	if(event->events & EPOLLIN){
		conn->read();
	}
	if(event->events & EPOLLOUT){
		conn->write(conn->get_rdbuf());
	}
	reinstall_fd(conn->_conn_fd, EPOLLIN | EPOLLOUT, conn);
	/*struct epoll_event ev;
	ev.events = EPOLLIN | EPOLLOUT | EPOLLONESHOT;
	ev.data.ptr = conn;
	cout << "reinstall fd=" << conn->_conn_fd << endl;
	if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, conn->_conn_fd, &ev) == -1) {
		perror("epoll_ctl: EPOLL_CTL_MOD1");
		_exit(-1);
	}*/
}

void TCPServer::install_fd(int fd, int events, void* data){
	struct epoll_event ev;
	ev.events = events;
	ev.data.ptr = data;
	cout << "install fd=" << fd << endl;
	if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1) {
		perror("epoll_ctl: EPOLL_CTL_MOD");
		_exit(-1);
	}
}

void TCPServer::reinstall_fd(int fd, int events){
	struct epoll_event ev;
	ev.events = events;
	ev.data.fd = fd;
	cout << "reinstall fd=" << fd << endl;
	if (epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
		perror("epoll_ctl: EPOLL_CTL_MOD");
		_exit(-1);
	}
}

void TCPServer::reinstall_fd(int fd, int events, void* data){
	struct epoll_event ev;
	ev.events = events;
	ev.data.ptr = data;
	cout << "reinstall fd=" << fd << endl;
	if (epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
		perror("epoll_ctl: EPOLL_CTL_MOD");
		_exit(-1);
	}
}

void TCPServer::do_use_fd(struct epoll_event* event){

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

void TCPServer::set_socket_buffer(int fd, int size, int opt){
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

void TCPServer::set_nonblocking(int fd){
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

void TCPServer::bind_socket_listen(int fd){
	const int on = 1;
	if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof (on) ) < 0){
		cout << "bind() error: " << strerror(errno) << endl;;
	}

	struct sockaddr_in addr;
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(_port);
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


void TCPServer::dowork(){
	struct epoll_event events[MAX_EVENTS];
    int nfds;
    while (!_exiting) {
    	cout << "epoll_wait thrd " << pthread_self()<< endl;
        nfds = epoll_wait(_epoll_fd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            cout << "epoll_wait failed" << endl;
            perror("epoll_wait");
            if(errno == EINTR){
            	cout << "_exiting = " << _exiting << endl;
            	continue;
            }
            else{
		cout << "epoll_wait errno "<< errno << endl;
            	break;
            }
        }
        cout << "epoll_wait woke up for " << pthread_self() << endl;
        struct sockaddr_in local;
        socklen_t addrlen = sizeof(local);
        int conn_fd;
        for (int n = 0; n < nfds; ++n) {
            if (events[n].data.fd == _listen_fd) {
				cout << "accepting on fd " << _listen_fd << endl;
                conn_fd = accept4(_listen_fd, (struct sockaddr *) &local, &addrlen, SOCK_NONBLOCK);
                if (conn_fd == -1) {
                	if(errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR || errno == ECONNABORTED){
                		continue;
                	}
                    perror("accept");
                    cout << "accept() error: " << errno;
                    _exit(-1);
                }
				cout << "conn_fd="<< conn_fd << endl;
                // set send buffer to the lowest limit, for testing purpose
                set_socket_buffer(conn_fd, 2, SO_SNDBUF);

                Connection* new_conn = new Connection(conn_fd, _epoll_fd);
                cout << "add new conn " << new_conn <<", fd=" << conn_fd << endl;
				install_fd(conn_fd, EPOLLIN | EPOLLOUT | EPOLLET | EPOLLONESHOT, new_conn);
				/*
                ev.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLONESHOT;
                ev.data.ptr = new_conn;
                if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, conn_fd, &ev) == -1) {
                    perror("epoll_ctl: conn_sock1");
                    _exit(-1);
                }*/
				// reinstall listen fd
				reinstall_fd(_listen_fd, EPOLLIN | EPOLLET | EPOLLONESHOT);
				/*
				ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
                ev.data.fd = _listen_fd;
				
				cout << "reinstall fd=" << _listen_fd << endl;
                if (epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, _listen_fd, &ev) == -1) {
                    perror("epoll_ctl: conn_sock2");
                    _exit(-1);
                }*/
            } else {
                do_use_fd1(&events[n]);
            }
        }
    }
}


