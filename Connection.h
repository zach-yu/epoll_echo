/*
 * Connection.h
 *
 *  Created on: 2015年6月26日
 *      Author: yuze
 */

#ifndef CONNECTION_H_
#define CONNECTION_H_

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
#include "ByteBuffer.h"
#include <unordered_map>
#include <vector>
#include <memory>

using namespace std;

class Connection {
public:
	enum CONN_STATE{
		READY = 0,
		READING_HEADER,
		FINISHED_HEADER,
		READING_BODY,
		ERROR,
		CLOSED
	};

	Connection(int fd, int epollfd);
	virtual ~Connection();

	void setWriteBuffer(const shared_ptr<ByteBuffer>& buf){
		_wbuf = buf;
	}

	void setReadBuffer(const shared_ptr<ByteBuffer>& buf){
		_rbuf = buf;
	}

	int readMessageHeader(ByteBuffer& header_buf){
		_state = READING_HEADER;

		read(&header_buf);
		cout << "state: " << _state << endl;
		if(_state == CLOSED) {
			return 0;
		}
		else if(_state == ERROR){
			cout << strerror(errno) << endl;
			return -1;
		}

		// reinstall event
		if(wbufRemaining() > 0){
			register_event(_epoll_fd, this, EPOLLIN | EPOLLOUT);
		}
		else{
			register_event(_epoll_fd, this, EPOLLIN);
		}

		if(header_buf.remaining() > 0){
			// blocked
			cout << "make_shared" << endl;
			// buffer will be copied here. the buffer copy can be elliminated if
			// we allocate it on heap.
			auto header_buf_ptr = make_shared<ByteBuffer>(header_buf);
			setReadBuffer(header_buf_ptr);
			return -1;
		}
		else{
			cout << "state: FINISHED_HEADER" << endl;
			_state = FINISHED_HEADER;
			return 1;
		}
	}

	int readMessageBody(ByteBuffer& body_buf){
		_state = READING_BODY;


		int ret = read(&body_buf);
		cout << "read body bytes:" << ret << endl;
		if(_state == CLOSED) {
			return 0;
		}
		else if(_state == ERROR){
			cout << strerror(errno) << endl;
			return -1;
		}

		if(wbufRemaining() > 0){
			register_event(_epoll_fd, this, EPOLLIN | EPOLLOUT);
		}
		else{
			register_event(_epoll_fd, this, EPOLLIN);
		}

		if(body_buf.remaining() > 0){
			// blocked
			// reinstall event
			cout << "make_shared" << endl;
			auto body_buf_ptr = make_shared<ByteBuffer>(body_buf);
			setReadBuffer(body_buf_ptr);
			return -1;
		}
		else{
			_state = READY;
			return 1;
		}

	}

	int read(ByteBuffer* buf);
	int write(ByteBuffer* buf);

	int writeRemaining(){
		return write(_wbuf.get());
	}

	int wbufRemaining(){
		if(_wbuf)
			return _wbuf->remaining();
		else
			return 0;
	}

	int readRemaining(){
		int ret =read(_rbuf.get());
		if(_state == ERROR || _state == CLOSED)
			return ret;
		if(_rbuf->remaining() == 0){
			if(_state == READING_HEADER){
				_state = FINISHED_HEADER;
			}
			else if(_state == READING_BODY){
				_state = READY;
			}
		}
		else{
			cout << "have to read: " << _rbuf->remaining() << endl;
			if(wbufRemaining() > 0){
				register_event(_epoll_fd, this, EPOLLIN | EPOLLOUT);
			}
			else{
				register_event(_epoll_fd, this, EPOLLIN);
			}

		}
		return ret;
	}

	int rbufRemaining(){
		if(_rbuf)
			return _rbuf->remaining();
		else
			return 0;
	}

	int _conn_fd;
	CONN_STATE _state;
private:
	//int _conn_fd;
	int _epoll_fd;

	// these pointers are used to temporarily store
	// partially written/read buffer pointers
	shared_ptr<ByteBuffer> _wbuf;
	shared_ptr<ByteBuffer> _rbuf;

	// register ONESHOT event
	int register_event(int epollfd, Connection* conn, int events){
		cout << "register event IN "<< (int)(events&EPOLLIN) << " OUT " << (int)(events&EPOLLOUT) <<" on conn " << conn << endl;
		struct epoll_event ev;
		// is EPOLLET needed, if we have EPOLLONESHOT?
	    ev.events = events | EPOLLET | EPOLLONESHOT;
	    ev.data.ptr = conn;
		if (epoll_ctl(epollfd, EPOLL_CTL_MOD, conn->_conn_fd, &ev) == -1) {
			cout << " EPOLL_CTL_MOD failed" << endl;
			return -1;
		}
		return 0;
	}

};

typedef std::unordered_map<int, Connection*> ConnMap;


#endif /* CONNECTION_H_ */
