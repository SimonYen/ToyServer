#ifndef OTHER_H
#define OTHER_H

#include "timer.h"

struct client_data
{
	sockaddr_in address;
	int sockfd;
	Timer *timer;
};

// 回调函数
void callback_func(client_data *user_data);

#endif