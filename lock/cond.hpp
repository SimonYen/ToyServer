#ifndef COND_HPP
#define COND_HPP

#include <exception>
#include <pthread.h>

/*
该文件封装了条件变量类
*/

class Cond
{
private:
	pthread_cond_t cond;

public:
	// 初始化条件变量
	Cond()
	{
		if (pthread_cond_init(&cond, nullptr) != 0)
			throw std::runtime_error("无法初始化条件变量！");
	}
	// 销毁条件变量
	~Cond()
	{
		pthread_cond_destroy(&cond);
	}
	// 等待目标条件变量，调用该函数时，必须确保mutex加锁
	bool wait(pthread_mutex_t *mutex)
	{
		return pthread_cond_wait(&cond, mutex) == 0;
	}
	bool timedwait(pthread_mutex_t *mutex, timespec *ts)
	{
		return pthread_cond_timedwait(&cond, mutex, ts) == 0;
	}
	// 唤醒一个等待条件变量的线程
	bool signal()
	{
		return pthread_cond_signal(&cond) == 0;
	}
	// 广播，唤醒所有等待条件变量的线程
	bool broadcast()
	{
		return pthread_cond_broadcast(&cond) == 0;
	}
};

#endif