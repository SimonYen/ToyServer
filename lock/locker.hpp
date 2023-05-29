#ifndef LOCKER_HPP
#define LOCKER_HPP

#include <pthread.h>
#include <exception>

/*
该文件封装了互斥锁类
*/

class Locker
{
private:
	pthread_mutex_t mutex;

public:
	// 初始化互斥锁
	Locker()
	{
		if (pthread_mutex_init(&mutex, nullptr) != 0)
			throw std::runtime_error("无法初始化互斥锁！");
	}
	// 销毁互斥锁
	~Locker()
	{
		pthread_mutex_destroy(&mutex);
	}
	// 加锁
	bool lock()
	{
		// 如果此时已经被锁上，那么该操作将会被阻塞，直到锁的占有者释放该锁
		return pthread_mutex_lock(&mutex) == 0;
	}
	// 解锁
	bool unlock()
	{
		return pthread_mutex_unlock(&mutex) == 0;
	}
	// 获取互斥锁指针
	pthread_mutex_t *get()
	{
		return &mutex;
	}
};

#endif