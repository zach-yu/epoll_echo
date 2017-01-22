/*
 * Connection.cpp
 *
 *  Created on: 2015年6月26日
 *      Author: yuze
 */
#include <unistd.h>
#include "Connection.h"
#include <iostream>

Connection::Connection(int fd, int epollfd) :  _conn_fd(fd), _state(Connection::READY), 
_epoll_fd(epollfd), _rbuf(new ByteBuffer(64)) {
}

Connection::~Connection() {
	std::cout << "in Connection::~Connection()" << std::endl;
	close(_conn_fd);
}

ByteBuffer* Connection::get_rdbuf(){
	return this->_rbuf.get();
}

int Connection::read(){
	return read(this->_rbuf.get());
}


int Connection::read(ByteBuffer* buf){
	
	int nread = 0;
	int to_read = buf->_limit - (buf->_rpos + 1);
	while (nread < to_read){
		int ret = ::read(_conn_fd, buf->_buff + buf->_rpos + nread, to_read - nread);
		if(ret < 0){
			if(errno == EINTR){
				continue;
			}
			else if(errno == EWOULDBLOCK || errno == EAGAIN){
				break;
			}
			else{
				cout << "read error:" << strerror(errno) << endl;
				_state = ERROR;
				break;
			}
		}
		else if(ret == 0){
			cout << "read EOF"<< endl;
			_state = CLOSED;
			break;
		}
		else{
			nread += ret;
		}
		buf->_rpos += nread;
	}
	cout << "Connection read " << nread << endl;
	return nread;

}

// if buf.remain() > 0 after calling Connection::write(), there is some
// pending error: maybe EWOULDBLOCK, EPIPE, ECONRESET, etc. should check
// errno after calling this function
int Connection::write(ByteBuffer* buf){
	int nwrite = 0;
	int to_write = buf->_rpos - buf->_wpos;
	while(nwrite < to_write){
		int ret = ::write(_conn_fd, buf->_buff + buf->_wpos + nwrite, to_write - nwrite);
		if(ret < 0){
			if(errno == EINTR){
				continue;
			}
			else if(errno == EWOULDBLOCK || errno == EAGAIN){
				break;
			}
			else{
				_state = ERROR;
			}
		}
		else{
			nwrite += ret;
		}
	}
	buf->_wpos += nwrite;
	if(buf->_wpos == buf->_rpos) buf->_rpos = buf->_wpos = -1;
	cout << "Connection write " << nwrite << endl;
	return nwrite;

}



