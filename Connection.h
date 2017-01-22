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
#include <deque>
#include <mutex>

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

/*
	void setReadBuffer(const shared_ptr<ByteBuffer>& buf){
		_rbuf = buf;
	}

	shared_ptr<ByteBuffer> getReadBuffer(){
		return _rbuf;
	}
*/
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
		if(_wbuf_queue.size() > 0){
			register_event(EPOLLIN | EPOLLOUT);
		}
		else{
			register_event(EPOLLIN);
		}

		if(header_buf.remaining() == 0){
			cout << "state: FINISHED_HEADER" << endl;
			_state = FINISHED_HEADER;
			return 1;
		}
		else{
			return -1;
		}
	}

	int readMessageBody(ByteBuffer& body_buf){
		_state = READING_BODY;
		int ret = read(&body_buf);
		//cout << "read body bytes:" << ret << ", body:" << string((const char *)body_buf._buff, body_buf._pos - body_buf._buff) << endl;
		if(_state == CLOSED) {
			return 0;
		}
		else if(_state == ERROR){
			cout << strerror(errno) << endl;
			return -1;
		}

		if(_wbuf_queue.size()  > 0){
			register_event( EPOLLIN | EPOLLOUT);
		}
		else{
			register_event(EPOLLIN);
		}

		if(body_buf.remaining() == 0){
			_state = READY;
			return 1;
		}
		else{
			return -1;
		}
	}

	int read(ByteBuffer* buf);
	int write(ByteBuffer* buf);

	int readRemaining(){
		int ret =read(_rbuf);
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
			//cout << "rbuf: "<< string((const char *)_rbuf->_buff, _rbuf->_pos - _rbuf->_buff) << " have to read: " << _rbuf->remaining() << endl;
			if(_wbuf_queue.size() > 0){
				register_event(EPOLLIN | EPOLLOUT);
			}
			else{
				register_event(EPOLLIN);
			}

		}
		return ret;
	}

	int processHeader(const ByteBuffer& buff){
		//TODO:
		cout << "reived header:" << string((const char *)buff._buff, buff._limit) << endl;
		return 1;
	}

	int processHeader(){
		//TODO:
		cout << "reived header:" << string((const char *)_rbuf->_buff, _rbuf->_limit) << endl;
		return 1;
	}

	int processBody(){
		cout << "received body:" << string((const char *)_rbuf->_buff, _rbuf->_limit) << endl;
		// decode body and call processor
		int to_write = 0;
		// for testing blocking write
		unsigned char *WBUF = new unsigned char[12800];
		size_t nread = _rbuf->_limit;
		for( unsigned char* p = WBUF; p + nread < WBUF + 12800; p+= nread){
			memcpy(p, _rbuf->_buff, nread);
			to_write += nread;
		}
		*(WBUF + to_write - 1) = 'X' ;
		//
		auto wbuf = make_shared<ByteBuffer>(ByteBuffer());
		wbuf->setBuffer(WBUF, to_write);
		addWBuffer(wbuf);
		register_event(EPOLLIN | EPOLLOUT);
		return to_write;
	}

	int rbufRemaining(){
		if(_rbuf)
			return _rbuf->remaining();
		else
			return 0;
	}

	void addWBuffer(const shared_ptr<ByteBuffer>& buf){
		lock_guard<mutex> lock(_mutex);
		_wbuf_queue.push_back(buf);

	}

	int flushWBuffer(){
		int count = 0;
		lock_guard<mutex> lock(_mutex);
		while(!_wbuf_queue.empty()){
			auto buffer = _wbuf_queue.front();
			write(buffer.get());
			if(buffer->remaining() > 0){
				// blocked or error
				break;
			}
			else{
				_wbuf_queue.pop_front();
				++count;
			}
		}
		if(_wbuf_queue.empty()){
			cout << "remove write event" << endl;
			register_event(EPOLLIN);
		}
		else{
			cout << "blocked in flushWBUffer" << endl;
		}
		return count;
	}

	int _conn_fd;
	CONN_STATE _state;
private:
	//int _conn_fd;
	int _epoll_fd;

	// these pointers are used to temporarily store
	// partially written/read buffer pointers

	mutex _mutex;
	deque<shared_ptr<ByteBuffer>> _wbuf_queue;
	ByteBuffer* _rbuf;
	

public:
	
	ByteBuffer* get_rdbuf();
	int read();
	// register ONESHOT event
	int register_event(int events){
		cout << "register event IN "<< (int)(events&EPOLLIN) << " OUT " << (int)(events&EPOLLOUT) <<" on conn " << this << endl;
		struct epoll_event ev;
		// is EPOLLET needed, if we have EPOLLONESHOT?
	    ev.events = events | EPOLLET | EPOLLONESHOT;
	    ev.data.ptr = this;
		if (epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, _conn_fd, &ev) == -1) {
			cout << " EPOLL_CTL_MOD failed" << endl;
			return -1;
		}
		return 0;
	}

};

typedef std::unordered_map<int, Connection*> ConnMap;


#endif /* CONNECTION_H_ */
