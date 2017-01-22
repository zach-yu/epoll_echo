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

using namespace std;

class ByteBuffer {
public:
	ByteBuffer() : _buff(0), _sz(0), _limit(0), _rpos(-1), _wpos(-1){}
	ByteBuffer(size_t sz){
		_buff = new unsigned char[sz];
		_limit = _sz = sz;
		_rpos = _wpos = -1;
		cout << "create ByteBuffer " << sz << endl;
	}

	ByteBuffer(const ByteBuffer& buf){
		_buff = new unsigned char[buf._sz];
		_sz = buf._sz;
		memcpy(_buff, buf._buff, _sz);
		_limit = buf._limit;
		_rpos = buf._rpos;
		_wpos = buf._wpos;
		//std::cout << "ByteBuffer copy ctor, allocate at " << (void*) _buff << " size:" << _sz << " filled:" << _pos - _buff << std::endl;
	}

	ByteBuffer(ByteBuffer&& other) : _buff(other._buff), _sz(other._sz), _limit(other._limit), _rpos(other._rpos), _wpos(other._wpos){
		other._rpos = other._wpos = -1;
		other._limit = 0;
		other._sz = 0;
		other._buff = 0;
	}

	virtual ~ByteBuffer();

	int remaining(){
		return _limit - _rpos;
	}

	void setBuffer(unsigned char* buf, size_t sz){
		_buff = buf;
		_limit = _sz = sz;
		_rpos = _wpos = -1;
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
	int _rpos;
	int _wpos;

friend class Connection;
};

#endif /* BYTEBUFFER_H_ */
