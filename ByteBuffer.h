/*
 * ByteBuffer.h
 *
 *  Created on: 2015年6月26日
 *      Author: yuze
 */

#ifndef BYTEBUFFER_H_
#define BYTEBUFFER_H_

#include <stdlib.h>
#include <iostream>
#include <string.h>

class ByteBuffer {
public:
	ByteBuffer() : _buff(0), _sz(0), _limit(0), _pos(0){}

	ByteBuffer(size_t sz){

		_buff = new unsigned char[sz];
		std::cout << "allocate buf:" << (void*) _buff << std::endl;
		_limit = _sz = sz;
		_pos = _buff;
	}

	ByteBuffer(const ByteBuffer& buf){

		_buff = new unsigned char[buf._sz];

		_sz = buf._sz;
		memcpy(_buff, buf._buff, _sz);
		_limit = buf._limit;
		_pos = _buff + (buf._pos - buf._buff);
		std::cout << "ByteBuffer copy ctor, allocate at " << (void*) _buff << " size:" << _sz << " filled:" << _pos - _buff << std::endl;
	}

	ByteBuffer(ByteBuffer&& other) : _buff(other._buff), _sz(other._sz), _limit(other._limit), _pos(other._pos){
		other._pos = 0;
		other._limit = 0;
		other._sz = 0;
		other._buff = 0;
	}

	virtual ~ByteBuffer();

	int remaining(){
		return _buff + _limit - _pos;
	}

	void setBuffer(unsigned char* buf, size_t sz){
		_pos = _buff = buf;
		_limit = _sz = sz;
	}

	const unsigned char* getBuffer() const{
		return _buff;
	}

	size_t getLimit() const{
		return _limit;
	}

protected:
	unsigned char* _buff;
	size_t _sz;
	size_t _limit;
	unsigned char* _pos;

friend class Connection;
};

#endif /* BYTEBUFFER_H_ */
