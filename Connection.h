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
#include <memory>

using namespace std;

class Connection {
public:
	Connection(int fd);
	virtual ~Connection();

	void setWriteBuffer(const shared_ptr<ByteBuffer>& buf){
		_wbuf = buf;
	}

	void setReadBuffer(const shared_ptr<ByteBuffer>& buf){
		_rbuf = buf;
	}

	int read(ByteBuffer* buf){ return 0;};
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

	int _conn_fd;
private:
	//int _conn_fd;
	// these pointers are used to temporarily store
	// partially written/read buffer pointers
	shared_ptr<ByteBuffer> _wbuf;
	shared_ptr<ByteBuffer> _rbuf;

};

typedef std::unordered_map<int, Connection*> ConnMap;


#endif /* CONNECTION_H_ */
