#pragma once

#include <mutex>
#include <deque>
#include <condition_variable>
#include <sys/time.h>
#include <cassert>

template <class T>
class BlockDeque
{
private:
	std::deque<T> deq_;

	size_t capacity_;

	std::mutex mtx_;

	bool isClose_;

	std::condition_variable condConsumer_;

	std::condition_variable condProducer_;

public:
	explicit BlockDeque(size_t MaxCapacity = 1000) : capacity_(MaxCapacity)
	{
		assert(MaxCapacity > 0);
		isClose_ = false;
	}
	// 关闭阻塞队列
	void Close()
	{
		{
			std::lock_guard<std::mutex> locker(mtx_);
			deq_.clear();
			isClose_ = true;
		}
		condProducer_.notify_all();
		condConsumer_.notify_all();
	}
	~BlockDeque()
	{
		Close();
	}
	inline void flush()
	{
		condConsumer_.notify_one();
	}
	// 清空队列
	void clear()
	{
		std::lock_guard<std::mutex> locker(mtx_);
		deq_.clear();
	}
	/*
	取出队列头部元素
	@return 头部元素
	*/
	T front()
	{
		std::lock_guard<std::mutex> locker(mtx_);
		return deq_.front();
	}
	/*
	取出队列尾部元素
	@return 尾部元素
	*/
	T back()
	{
		std::lock_guard<std::mutex> locker(mtx_);
		return deq_.back();
	}
	/*
	获取队列长度
	@return 队列长度
	*/
	size_t size()
	{
		std::lock_guard<std::mutex> locker(mtx_);
		return deq_.size();
	}
	/*
	获取队列容量
	@return 队列容量
	*/
	size_t capacity()
	{
		std::lock_guard<std::mutex> locker(mtx_);
		return deq_.size();
	}
	/*
	将元素放入队列尾部
	@param item 需要放入的元素
	*/
	void push_back(const T &item)
	{
		std::unique_lock<std::mutex> locker(mtx_);
		while (deq_.size() >= capacity_)
		{
			condProducer_.wait(locker); // 一直阻塞，直到消费者通知有空位置
		}
		deq_.push_back(item);
		condConsumer_.notify_one(); // 通知消费者可以消费了
	}
	/*
	将元素放入队列头部
	@param item 需要放入的元素
	*/
	void push_front(const T &item)
	{
		std::unique_lock<std::mutex> locker(mtx_);
		while (deq_.size() >= capacity_)
		{
			condProducer_.wait(locker); // 一直阻塞，直到消费者通知有空位置
		}
		deq_.push_front(item);
		condConsumer_.notify_one(); // 通知消费者可以消费了
	}
	/*
	检查队列是否为空
	@return 空为真，否则为假
	*/
	bool empty()
	{
		std::lock_guard<std::mutex> locker(mtx_);
		return deq_.empty();
	}
	/*
	检查队列是否已满
	@return 满为真，否则为假
	*/
	bool full()
	{
		std::lock_guard<std::mutex> locker(mtx_);
		return deq_.size() >= capacity_;
	}
	/*
	弹出队列头部元素
	@param item 弹出的元素会赋值给item
	@return 弹出是否成功
	*/
	bool pop(T &item)
	{
		std::unique_lock<std::mutex> locker(mtx_);
		while (deq_.empty())
		{
			condConsumer_.wait(locker);
			if (isClose_)
			{
				return false;
			}
		}
		item = deq_.front();
		deq_.pop_front();
		condProducer_.notify_one();
		return true;
	}
	/*
	弹出队列头部元素
	@param item 弹出的元素会赋值给item
	@param timeout 超时的时间，单位秒
	@return 弹出是否成功
	*/
	bool pop(T &item, int timeout)
	{
		std::unique_lock<std::mutex> locker(mtx_);
		while (deq_.empty())
		{
			if (condConsumer_.wait_for(locker, std::chrono::seconds(timeout)) == std::cv_status::timeout)
			{
				return false;
			}
			if (isClose_)
			{
				return false;
			}
		}
		item = deq_.front();
		deq_.pop_front();
		condProducer_.notify_one();
		return true;
	}
};