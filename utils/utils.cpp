#include "utils.h"

void Utils::init(int timeshot)
{
	TIMESLOT = timeshot;
}

int Utils::set_nonblocking(int fd)
{
	int old_option = fcntl(fd, F_GETFL);
	int new_option = old_option | O_NONBLOCK;
	fcntl(fd, F_SETFL, new_option);
	return old_option;
}

void Utils::add_fd(int epollfd, int fd, bool one_shot, int trigger_mode)
{
	epoll_event event;
	event.data.fd = fd;
	if (1 == trigger_mode)
		event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
	else
		event.events = EPOLLIN | EPOLLRDHUP;
	if (one_shot)
		event.events |= EPOLLONESHOT;
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
	set_nonblocking(fd);
}

void Utils::sig_handler(int sig)
{
	int save_errno = errno;
	int msg;
	send(pipe_fd[1], (char *)&msg, 1, 0);
	errno = save_errno;
}

void Utils::add_sig(int sig, void(handler)(int), bool restart)
{
	struct sigaction sa;
	memset(&sa, '\0', sizeof(sa));
	sa.sa_handler = handler;
	if (restart)
		sa.sa_flags |= SA_RESTART;
	sigfillset(&sa.sa_mask);
	assert(sigaction(sig, &sa, nullptr) != -1);
}

void Utils::timer_handler()
{
	timer_list.tick();
	alarm(TIMESLOT);
}

void Utils::show_error(int conn_fd, const char *info)
{
	send(conn_fd, info, strlen(info), 0);
	close(conn_fd);
}

int *Utils::pipe_fd = 0;
int Utils::epoll_fd = 0;

Utils::Utils()
{
}
Utils::~Utils()
{
}