#ifndef LOG_HPP
#define LOG_HPP

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iostream>
#include <string>
#include <pthread.h>
#include <sys/time.h>
#include "block_queue.hpp"

// 懒汉单例模式
class Logger
{
private:
	char path_name[128];			// 路径名
	char file_name[128];			// 日志文件名
	unsigned int max_lines;			// 日志最大行数
	unsigned int log_buf_size;		// 日志缓冲区大小
	unsigned long long lines_count; // 记录日志行数
	unsigned int today;
	FILE *file;							 // 文件指针
	char *buf;							 // 缓冲区
	BlockQueue<std::string> *log_queque; // 阻塞队列
	Locker mutex;
	bool is_async;
	int close_log;

private:
	Logger()
	{
		lines_count = 0;
		is_async = false;
	}
	virtual ~Logger()
	{
		if (file != nullptr)
			fclose(file);
	}
	// 异步写日志
	void *async_write_log()
	{
		std::string log;
		while (log_queque->pop(log))
		{
			mutex.lock();
			fputs(log.c_str(), file);
			mutex.unlock();
		}
	}

public:
	static Logger *get_instance()
	{
		static Logger instance;
		return &instance;
	}
	static void *flush_log_thread(void *args)
	{
		Logger::get_instance()->async_write_log();
	}
	bool init(const char *_file_name, int _close_log, int _log_buf_size = 8192, int _max_lines = 5000000, int _max_queue_size = 0)
	{
		// 如果设置阻塞队列大小为正数，那么就是开启异步
		if (_max_queue_size > 0)
		{
			is_async = true;
			log_queque = new BlockQueue<std::string>(_max_queue_size);
			// 创建一个线程用来写日志
			pthread_t tid;
			pthread_create(&tid, nullptr, flush_log_thread, nullptr);
		}
		close_log = _close_log;
		log_buf_size = _log_buf_size;
		buf = new char[log_buf_size];
		memset(buf, '\0', log_buf_size);
		max_lines = _max_lines;

		// 获取时间
		time_t t = time(nullptr);
		auto sys_tm = localtime(&t);

		// 反向查找'/'
		auto p = strrchr(_file_name, '/');
		char log_full_name[256] = {0};

		if (p)
		{
			// 将/后的文件名复制
			strcpy(file_name, p + 1);
			// 将/前的路径复制
			strncpy(path_name, _file_name, p - _file_name + 1);
			// 构造完整的文件名
			snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", path_name, sys_tm->tm_year + 1900, sys_tm->tm_mon + 1, sys_tm->tm_mday, file_name);
		}
		else
			snprintf(log_full_name, 255, "%d_%02d_%02d_%s", sys_tm->tm_year + 1900, sys_tm->tm_mon + 1, sys_tm->tm_mday, file_name);

		today = sys_tm->tm_mday;

		file = fopen(log_full_name, "a");
		return file == nullptr ? false : true;
	}
	void write_log(int _level, const char *format, ...)
	{
	}
	void flush()
	{
	}
};
#endif