#ifndef LST_TIMER
#define LST_TIMER

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <time.h>

#include "../log/logger.hpp"

// 定时器
class Timer
{
public:
	time_t expire;
	void (*callback_func)(client_data *); // 应该是函数指针
	client_data *user_data;
	Timer *prev;
	Timer *next;
	Timer();
};

// 定时器链表
class TimerList
{
private:
	Timer *head, *tail;
	void add_timer(Timer *timer, Timer *list_head);

public:
	TimerList();
	~TimerList();
	void add_timer(Timer *timer);
	void adjust_timer(Timer *timer);
	void del_timer(Timer *timer);
	void tick();
};

#endif