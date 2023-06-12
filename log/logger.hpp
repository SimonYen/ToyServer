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
	bool log_on; // 是否开启日志

private:
	Logger()
	{
		lines_count = 0;
		is_async = false;
		log_on = false;
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
	bool init(const char *_file_name, int _log_buf_size = 8192, int _max_lines = 5000000, int _max_queue_size = 0, bool _log_on)
	{
		log_on = _log_on;
		// 如果设置阻塞队列大小为正数，那么就是开启异步
		if (_max_queue_size > 0)
		{
			is_async = true;
			log_queque = new BlockQueue<std::string>(_max_queue_size);
			// 创建一个线程用来写日志
			pthread_t tid;
			pthread_create(&tid, nullptr, flush_log_thread, nullptr);
		}
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
	// 写日志
	void write_log(int _level, const char *format, ...)
	{
		if (!log_on)
			return;
		// 获取当前时间
		timeval now = {0, 0};
		gettimeofday(&now, nullptr);
		auto t = now.tv_sec;
		auto sys_tm = localtime(&t);
		char s[16] = {0};
		switch (_level)
		{
		case 0:
			strcpy(s, "[DEBUG]:");
			break;
		case 1:
			strcpy(s, "[INFO]:");
			break;
		case 2:
			strcpy(s, "[WARN]:");
			break;
		case 3:
			strcpy(s, "[ERROR]:");
			break;

		default:
			strcpy(s, "[INFO]:");
			break;
		}
		mutex.lock();
		lines_count++;
		// 日志行数并未达到最大值，并且当前天数已经不是创建Logger那天的天数
		if (today != sys_tm->tm_mday || lines_count % max_lines == 0) // 这里应该是&&，我觉得，但是为了保险还是用原来的这个，毕竟只是为了找工作
		{
			// 新文件名
			char new_log[256] = {0};
			fflush(file);
			fclose(file);
			char tail[16] = {0};
			snprintf(tail, 16, "%d_%02d_%02d_", sys_tm->tm_year + 1900, sys_tm->tm_mon + 1, sys_tm->tm_mday);

			if (today != sys_tm->tm_mday)
			{
				snprintf(new_log, 255, "%s%s%s", path_name, tail, file_name);
				// 更新日期
				today = sys_tm->tm_mday;
				lines_count = 0;
			}
			else
				snprintf(new_log, 255, "%s%s%s.%lld", path_name, tail, file_name, lines_count / max_lines);
			// 创建新文件
			file = fopen(new_log, "a");
		}
		mutex.unlock();
		// 可变参数
		va_list valst;
		va_start(valst, format);

		std::string log_str;
		mutex.lock();

		int n = snprintf(buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
						 sys_tm->tm_year + 1900,
						 sys_tm->tm_mon + 1,
						 sys_tm->tm_mday,
						 sys_tm->tm_hour, sys_tm->tm_min, sys_tm->tm_sec, now.tv_usec, s);
		int m = vsnprintf(buf + n, log_buf_size - n - 1, format, valst);
		buf[n + m] = '\n';
		buf[n + m + 1] = '\0';
		log_str = buf;
		mutex.unlock();
		if (is_async && !log_queque->full())
			log_queque->push(log_str);
		else // 直接写入文件
		{
			mutex.lock();
			fputs(log_str.c_str(), file);
			mutex.unlock();
		}
		va_end(valst);
	}
	void flush()
	{
		mutex.lock();
		fflush(file);
		mutex.unlock();
	}
};

#define LOG_DEBUG(format, ...)                                   \
	Logger::get_instance()->write_log(0, format, ##__VA_ARGS__); \
	Logger::get_instance()->flush();

#define LOG_INFO(format, ...)                                    \
	Logger::get_instance()->write_log(1, format, ##__VA_ARGS__); \
	Logger::get_instance()->flush();

#define LOG_WARN(format, ...)                                    \
	Logger::get_instance()->write_log(2, format, ##__VA_ARGS__); \
	Logger::get_instance()->flush();

#define LOG_ERROR(format, ...)                                   \
	Logger::get_instance()->write_log(3, format, ##__VA_ARGS__); \
	Logger::get_instance()->flush();

#endif