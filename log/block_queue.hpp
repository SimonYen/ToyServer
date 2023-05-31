#ifndef BLOCK_QUEUE
#define BLOCK_QUEUE

#include <iostream>
#include <exception>
#include <pthread.h>
#include <sys/time.h>
#include "../lock/cond.hpp"
#include "../lock/locker.hpp"

template <typename T>
class BlockQueue
{
private:
	Locker mutex;
	Cond cond;

	T *array;
	int size, max_size, head, tail;

public:
	BlockQueue(int _max_size = 1000)
	{
		if (_max_size <= 0)
			throw std::invalid_argument("阻塞队列初始化大小必须是正数！");
		max_size = _max_size;
		array = new T[max_size];
		size = 0;
		head = -1;
		tail = -1;
	}
	~BlockQueue()
	{
		mutex.lock();
		if (array != nullptr)
			delete[] array;
		mutex.unlock();
	}
	// 清空队列
	void clear()
	{
		mutex.lock();
		size = 0;
		head = -1;
		tail = -1;
		mutex.unlock();
	}
	// 判断队列是否已满
	bool full()
	{
		mutex.lock();
		if (size >= max_size)
		{
			mutex.unlock();
			return true;
		}
		mutex.unlock();
		return false;
	}
	// 判断是否为空
	bool empty()
	{
		mutex.lock();
		if (size == 0)
		{
			mutex.unlock();
			return true;
		}
		mutex.unlock();
		return false;
	}
	// 返回队首元素
	bool front(T &value)
	{
		mutex.lock();
		if (size == 0)
		{
			mutex.unlock();
			return false;
		}
		value = array[head];
		mutex.unlock();
		return true;
	}
	// 返回队尾元素
	bool back(T &value)
	{
		mutex.lock();
		if (size == 0)
		{
			mutex.unlock();
			return false;
		}
		value = array[tail];
		mutex.unlock();
		return true;
	}
	int size()
	{
		int tmp = 0;
		mutex.lock();
		tmp = size;
		mutex.unlock();
		return tmp;
	}
	int max_size()
	{
		int tmp = 0;
		mutex.lock();
		tmp = max_size;
		mutex.unlock();
		return tmp;
	}
	bool push(const T &item)
	{
		mutex.lock();
		// 当队列满了
		if (size >= max_size)
		{
			cond.broadcast(); // 通知等待条件变量的线程前来消费
			mutex.unlock();
			return false;
		}
		// 未满
		tail = (tail + 1) % max_size;
		array[tail] = item;
		size++;
		cond.broadcast();
		mutex.unlock();
		return true;
	}
	bool pop(T &item)
	{
		mutex.lock();
		// 当队列为空时
		while (size <= 0)
		{
			// 等待条件变量
			if (!cond.wait(mutex.get()))
			{
				mutex.unlock();
				return false;
			}
		}
		head = (head + 1) % max_size;
		item = array[head];
		size--;
		mutex.unlock();
		return true;
	}
	// 加入了超时处理
	bool pop(T &item, int ms_timeout)
	{
		timespec t = {0, 0};
		timeval now = {0, 0};
		gettimeofday(&now, nullptr);
		mutex.lock();
		if (size <= 0)
		{
			t.tv_sec = now.tv_sec + ms_timeout / 1000;
			t.tv_nsec = (ms_timeout % 1000) * 1000;
			if (!cond.timedwait(mutex.get(), t))
			{
				mutex.unlock();
				return false;
			}
		}
		if (size <= 0)
		{
			mutex.unlock();
			return false;
		}
		head = (head + 1) % max_size;
		item = array[head];
		size--;
		mutex.unlock();
		return true;
	}
};
#endif