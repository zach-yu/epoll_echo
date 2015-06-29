/*
 * Connection.h
 *
 *  Created on: 2015年6月26日
 *      Author: yuze
 */

#ifndef CONNECTION_H_
#define CONNECTION_H_

#include "ByteBuffer.h"
#include <unordered_map>
#include <vector>

class Connection {
public:
	Connection(int fd);
	virtual ~Connection();

	void setBuffer(ByteBuffer* buf){
		_wbuf = buf;
	}

	int read(ByteBuffer* buf){ return 0;};
	int write(ByteBuffer* buf);

	int writeRemaining(){
		return write(_wbuf);
	}

	int wbufRemaining(){
		if(_wbuf)
			return _wbuf->remaining();
		else
			return 0;
	}

	void freeWbuf(){
		delete _wbuf;
		_wbuf = 0;
	}

	int _conn_fd;
private:
	//int _conn_fd;
	//ConnMap* _conn_map;
	ByteBuffer* _wbuf;
};

typedef std::unordered_map<int, Connection*> ConnMap;


#endif /* CONNECTION_H_ */
