/*
 * ByteBuffer.cpp
 *
 *  Created on: 2015年6月26日
 *      Author: yuze
 */

#include "ByteBuffer.h"
#include <iostream>

using namespace std;


ByteBuffer::~ByteBuffer() {
	if(_buff != nullptr){
		cout << "ByteBuffer::~ByteBuffer(), delete:" << (void*)_buff << endl;
		delete[] _buff;
	}
}

