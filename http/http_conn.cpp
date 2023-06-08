#include "http_conn.h"
#include "../log/logger.hpp"

#include <map>
#include <mysql/mysql.h>
#include <fstream>

// #define connfdET //边缘触发非阻塞
#define connfdLT // 水平触发阻塞

// #define listenfdET //边缘触发非阻塞
#define listenfdLT // 水平触发阻塞

// 定义http响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

// 当浏览器出现连接重置时，可能是网站根目录出错或http响应格式出错或者访问的文件中内容完全为空
const char *doc_root = "/home/simon/TinyWebServer/root";

// 将表中的用户名和密码放入map
std::map<std::string, std::string> users;
Locker m_lock;

void HttpConnection::init_mysql_result(ConnectionPool *connPool)
{
	// 先从连接池中取一个连接
	MYSQL *mysql = NULL;
	ConnectionRAII mysqlcon(&mysql, connPool);

	// 在user表中检索username，passwd数据，浏览器端输入
	if (mysql_query(mysql, "SELECT username,passwd FROM user"))
	{
		LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
	}

	// 从表中检索完整的结果集
	MYSQL_RES *result = mysql_store_result(mysql);

	// 返回结果集中的列数
	int num_fields = mysql_num_fields(result);

	// 返回所有字段结构的数组
	MYSQL_FIELD *fields = mysql_fetch_fields(result);

	// 从结果集中获取下一行，将对应的用户名和密码，存入map中
	while (MYSQL_ROW row = mysql_fetch_row(result))
	{
		std::string temp1(row[0]);
		std::string temp2(row[1]);
		users[temp1] = temp2;
	}
}

// 对文件描述符设置非阻塞
int setnonblocking(int fd)
{
	int old_option = fcntl(fd, F_GETFL);
	int new_option = old_option | O_NONBLOCK;
	fcntl(fd, F_SETFL, new_option);
	return old_option;
}

// 将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void addfd(int epollfd, int fd, bool one_shot)
{
	epoll_event event;
	event.data.fd = fd;

#ifdef connfdET
	event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
#endif

#ifdef connfdLT
	event.events = EPOLLIN | EPOLLRDHUP;
#endif

#ifdef listenfdET
	event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
#endif

#ifdef listenfdLT
	event.events = EPOLLIN | EPOLLRDHUP;
#endif

	if (one_shot)
		event.events |= EPOLLONESHOT;
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
	setnonblocking(fd);
}

// 从内核时间表删除描述符
void removefd(int epollfd, int fd)
{
	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
	close(fd);
}

// 将事件重置为EPOLLONESHOT
void modfd(int epollfd, int fd, int ev)
{
	epoll_event event;
	event.data.fd = fd;

#ifdef connfdET
	event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
#endif

#ifdef connfdLT
	event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
#endif

	epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

int HttpConnection::user_count = 0;
int HttpConnection::epoll_fd = -1;

// 关闭连接，关闭一个连接，客户总量减一
void HttpConnection::close_conn(bool real_close)
{
	if (real_close && (sockfd != -1))
	{
		removefd(epoll_fd, sockfd);
		sockfd = -1;
		user_count--;
	}
}

// 初始化连接,外部调用初始化套接字地址
void HttpConnection::init(int _sockfd, const sockaddr_in &addr)
{
	sockfd = _sockfd;
	address = addr;
	// int reuse=1;
	// setsockopt(m_sockfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
	addfd(epoll_fd, sockfd, true);
	user_count++;
	log_on = true;
	init();
}

// 初始化新接受的连接
// check_state默认为分析请求行状态
void HttpConnection::init()
{
	mysql = nullptr;
	bytes_to_send = 0;
	bytes_have_send = 0;
	check_state = CHECK_STATE_REQUESTLINE;
	linger = false;
	method = GET;
	url = 0;
	version = 0;
	content_length = 0;
	host = 0;
	start_line = 0;
	checked_idx = 0;
	read_idx = 0;
	write_idx = 0;
	cgi = 0;
	memset(read_buf, '\0', READ_BUFFER_SIZE);
	memset(write_buf, '\0', WRITE_BUFFER_SIZE);
	memset(real_file, '\0', FILENAME_LEN);
}

// 从状态机，用于分析出一行内容
// 返回值为行的读取状态，有LINE_OK,LINE_BAD,LINE_OPEN
HttpConnection::LINE_STATUS HttpConnection::parse_line()
{
	char temp;
	for (; checked_idx < read_idx; ++checked_idx)
	{
		temp = read_buf[checked_idx];
		if (temp == '\r')
		{
			if ((checked_idx + 1) == read_idx)
				return LINE_OPEN;
			else if (read_buf[checked_idx + 1] == '\n')
			{
				read_buf[checked_idx++] = '\0';
				read_buf[checked_idx++] = '\0';
				return LINE_OK;
			}
			return LINE_BAD;
		}
		else if (temp == '\n')
		{
			if (checked_idx > 1 && read_buf[checked_idx - 1] == '\r')
			{
				read_buf[checked_idx - 1] = '\0';
				read_buf[checked_idx++] = '\0';
				return LINE_OK;
			}
			return LINE_BAD;
		}
	}
	return LINE_OPEN;
}