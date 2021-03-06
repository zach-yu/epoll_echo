/*
 * ExecutorService.h
 *
 *  Created on: 2015年7月1日
 *      Author: yuze
 */

#ifndef EXCUTORSERVICE_H_
#define EXCUTORSERVICE_H_
#include <vector>
#include <mutex>
#include <condition_variable>
#include <future>
#include <thread>
#include <deque>
#include <iostream>

using namespace std;

// Task should be Callable, and can get_future()
template<class Result, class Task>
class ExecutorService {
public:
	ExecutorService(size_t nthread) : _nthread(nthread), _exit(0){
		for(size_t i = 0; i < _nthread; ++i){
			_thread_pool.push_back(thread(&ExecutorService<Result, Task>::thread_func, this));
		}
	}
	virtual ~ExecutorService(){
		shutdown();
	}

	std::future<Result> submit(Task& task){
		std::unique_lock<std::mutex> lock(_mutex);
		auto f = task.get_future();
		_task_que.push_back(move(task));
		lock.unlock();
		_cond.notify_one();
		return f;
	}

	void shutdown(){
		std::unique_lock<std::mutex> lock(_mutex);
		_exit = 1;
		lock.unlock();
		_cond.notify_all();
		for(auto& thr : _thread_pool){
			thr.join();
		}
	}

private:
	size_t _nthread;
	int _exit;
	vector<thread> _thread_pool;
	mutex _mutex;
	condition_variable _cond;
	deque<Task> _task_que;

	void thread_func(){
		while(!_exit){
			std::unique_lock<std::mutex> lock(_mutex);
			if(!_task_que.empty()){
				//cout << "executing task" << endl;
				auto task = move(_task_que.front());
				_task_que.pop_front();
				task();
			}
			else{
				_cond.wait(lock);
			}
		}
	}


};

#endif /* EXCUTORSERVICE_H_ */
