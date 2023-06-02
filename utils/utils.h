#ifndef UTILS_H
#define UTILS_H

#include "timer.h"

class Utils
{
public:
	static int *pipe_fd;
	static int epoll_fd;
	int TIMESLOT;
	TimerList timer_list;

public:
	Utils();
	~Utils();
	void init(int timeslot);
	// 对文件描述符设置非阻塞
	int set_nonblocking(int fd);
	// 将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
	void add_fd(int epoll_fd, int fd, bool one_shot, int trigger_mode);
	// 信号处理函数
	static void sig_handler(int sig);
	// 设置信号函数
	void add_sig(int sig, void(handler)(int), bool restart = true);
	// 定时处理任务，重新定时以不断触发SIGALRM信号
	void timer_handler();
	void show_error(int conn_fd, const char *info);
};

#endif