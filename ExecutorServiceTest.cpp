/*
 * ExecutorService.cpp
 *
 *  Created on: 2015年7月1日
 *      Author: yuze
 */

#include "ExecutorService.h"

#include <iostream>

using namespace std;

class AddTask{
	int _a, _b;
public:
	AddTask(int a, int b) : _a(a), _b(b){

	}

	int operator()() const{
		return _a + _b;
	}
};


int add(int a, int b){
	return a+b;
}
int main(){
	ExecutorService<int, packaged_task<int()>> es;
	packaged_task<int()> pack_task (AddTask(1,2));
	future<int> f = es.submit(pack_task);
	packaged_task<int()> pack_task1 (AddTask(4,2));
	future<int> f1 = es.submit(pack_task1);

	cout << "0:" << f.get() << endl;
	cout << "1:" << f1.get() << endl;
/*
	future<int> f1 = es.submit([]() {return 9;});
	cout << f1.get() << endl;
*/
}
