#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#include <cstdio>
#include <list>
#include <exception>
#include <pthread.h>
#include "../lock/locker.hpp"
#include "../lock/sem.hpp"
#include "./mysql_connection_pool.hpp"

template <typename T>
class ThreadPool
{
private:
	int thread_number;		  // 线程池中的线程数
	int max_requests;		  // 请求队列中允许的最大请求数
	pthread_t *threads;		  // 描述线程池的数组
	std::list<T *> workqueue; // 请求队列
	Locker mutex;
	Sem sem;
	ConnectionPool *conn_pool; // 数据库连接池
	int actor_model;		   // 模型切换

	// 工作线程运行的函数，必须是静态函数,其实就是包装了run函数
	static void *worker(void *arg)
	{
		ThreadPool *pool = (ThreadPool *)arg;
		pool->run();
		return pool;
	}
	void run()
	{
		/*
			总结来说，就是每个线程不断的抢互斥锁，
			抢到了就从请求队列中拿请求，执行process函数
		*/
		while (true)
		{
			sem.wait();
			mutex.lock();
			// 检查请求队列是否为空
			if (workqueue.empty())
			{
				mutex.unlock();
				continue;
			}
			// 取出一个任务
			T *request = workqueue.front();
			workqueue.pop_front();
			mutex.unlock();
			if (!request)
				continue;
			if (1 == actor_model)
			{
				if (0 == request->state)
				{
					if (request->read_once())
					{
						request->improv = 1;
						ConnectionRAII mysql_con(&request->mysql, conn_pool);
						request->process();
					}
					else
					{
						request->improv = 1;
						request->timer_flag = 1;
					}
				}
				else
				{
					if (request->write())
						request->improv = 1;
					else
					{
						request->improv = 1;
						request->timer_flag = 1;
					}
				}
			}
			else
			{
				ConnectionRAII mysql_conn(&request->mysql, conn_pool);
				request->process();
			}
		}
	}

public:
	ThreadPool(int _actor_model, ConnectionPool *_conn_pool, int _thread_num = 8, int _max_request = 10000)
	{
		actor_model = _actor_model;
		conn_pool = _conn_pool;
		thread_number = _thread_num;
		max_requests = _max_request;
		if (thread_number <= 0 || max_request <= 0)
			throw std::invalid_argument("线程池中线程个数和最大请求量必须为正数！");
		threads = new pthread_t[thread_number];
		if (!threads)
			throw std::bad_alloc("无法创建线程数组！");
		// 创建线程池
		for (int i = 0; i < thread_number; i++)
		{
			if (pthread_create(threads + i, nullptr, worker, this) != 0)
			{
				delete[] threads;
				throw std::runtime_error("无法创建线程！");
			}
			if (pthread_detach(threads[i]))
			{
				delete[] threads;
				throw std::runtime_error("无法分离线程！");
			}
		}
	}
	~ThreadPool()
	{
		delete[] threads;
	}
	bool append(T *request, int state)
	{
		mutex.lock();
		if (workqueue.size() >= max_requests)
		{
			mutex.unlock();
			return false;
		}
		request->state = state;
		workqueue.push_back(request);
		mutex.unlock();
		sem.post();
		return true;
	}
	bool append_p(T *request)
	{
		mutex.lock();
		if (workqueue.size() >= max_requests)
		{
			mutex.unlock();
			return false;
		}
		workqueue.push_back(request);
		mutex.unlock();
		sem.post();
		return true;
	}
};

#endif