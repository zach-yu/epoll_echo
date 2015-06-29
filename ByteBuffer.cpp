/*
 * ByteBuffer.cpp
 *
 *  Created on: 2015年6月26日
 *      Author: yuze
 */

#include "ByteBuffer.h"

ByteBuffer::ByteBuffer(size_t sz) {
	_buff = new unsigned char[sz];
	_sz = sz;
	_limit = 0;
	_pos = 0;
}

ByteBuffer::~ByteBuffer() {
	if(_buff != nullptr){
		delete[] _buff;
		_sz = 0;
		_limit = 0;
		_pos = 0;

	}
}

