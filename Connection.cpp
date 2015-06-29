/*
 * Connection.cpp
 *
 *  Created on: 2015年6月26日
 *      Author: yuze
 */
#include <unistd.h>
#include "Connection.h"
#include <iostream>

Connection::Connection(int fd) : _conn_fd(fd) {
}

Connection::~Connection() {
	std::cout << "in Connection::~Connection()" << std::endl;
	close(_conn_fd);
}

// if buf.remain() > 0 after calling Connection::write(), there is some
// pending error: maybe EWOULDBLOCK, EPIPE, ECONRESET, etc. should check
// errno after calling this function
int Connection::write(ByteBuffer* buf){
	int nwrite = 0;
	int to_write = buf->_limit - (buf->_pos - buf->_buff);
	while(nwrite < to_write){
		int ret = ::write(_conn_fd, buf->_pos + nwrite, to_write - nwrite);
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
	buf->_pos += nwrite;
	return nwrite;

}



