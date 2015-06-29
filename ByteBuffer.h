/*
 * ByteBuffer.h
 *
 *  Created on: 2015年6月26日
 *      Author: yuze
 */

#ifndef BYTEBUFFER_H_
#define BYTEBUFFER_H_

#include <stdlib.h>

class ByteBuffer {
public:
	ByteBuffer() : _buff(0), _sz(0), _limit(0), _pos(0){}

	ByteBuffer(size_t sz);

	virtual ~ByteBuffer();

	int remaining(){
		return _buff + _limit - _pos;
	}

	void setBuffer(unsigned char* buf, size_t sz){
		_pos = _buff = buf;
		_limit = _sz = sz;
	}

protected:
	unsigned char* _buff;
	size_t _sz;
	size_t _limit;
	unsigned char* _pos;

friend class Connection;
};

#endif /* BYTEBUFFER_H_ */
