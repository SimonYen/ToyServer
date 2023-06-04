#ifndef OTHER_H
#define OTHER_H

#include "timer.h"
#include "utils.h"

struct client_data
{
	sockaddr_in address;
	int sockfd;
	Timer *timer;
};

// 回调函数
void callback_func(client_data *user_data)
{
	epoll_ctl(Utils::epoll_fd, EPOLL_CTL_DEL, user_data->sockfd, 0);
	assert(user_data);
	close(user_data->sockfd);
	http_conn::user_count--;
}

#endif