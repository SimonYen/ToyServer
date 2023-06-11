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

// 循环读取客户数据，直到无数据可读或对方关闭连接
// 非阻塞ET工作模式下，需要一次性将数据读完
bool HttpConnection::read_once()
{
	if (read_idx >= READ_BUFFER_SIZE)
	{
		return false;
	}
	int bytes_read = 0;

#ifdef connfdLT

	bytes_read = recv(sockfd, read_buf + read_idx, READ_BUFFER_SIZE - read_idx, 0);

	if (bytes_read <= 0)
	{
		return false;
	}

	read_idx += bytes_read;

	return true;

#endif

#ifdef connfdET
	while (true)
	{
		bytes_read = recv(sockfd, read_buf + read_idx, READ_BUFFER_SIZE - read_idx, 0);
		if (bytes_read == -1)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				break;
			return false;
		}
		else if (bytes_read == 0)
		{
			return false;
		}
		read_idx += bytes_read;
	}
	return true;
#endif
}

// 解析http请求行，获得请求方法，目标url及http版本号
HttpConnection::HTTP_CODE HttpConnection::parse_request_line(char *text)
{
	url = strpbrk(text, " \t");
	if (!url)
	{
		return BAD_REQUEST;
	}
	*url++ = '\0';
	char *p_method = text;
	if (strcasecmp(p_method, "GET") == 0)
		method = GET;
	else if (strcasecmp(p_method, "POST") == 0)
	{
		method = POST;
		cgi = 1;
	}
	else
		return BAD_REQUEST;
	url += strspn(url, " \t");
	version = strpbrk(url, " \t");
	if (!version)
		return BAD_REQUEST;
	*version++ = '\0';
	version += strspn(version, " \t");
	if (strcasecmp(version, "HTTP/1.1") != 0)
		return BAD_REQUEST;
	if (strncasecmp(url, "http://", 7) == 0)
	{
		url += 7;
		url = strchr(url, '/');
	}

	if (strncasecmp(url, "https://", 8) == 0)
	{
		url += 8;
		url = strchr(url, '/');
	}

	if (!url || url[0] != '/')
		return BAD_REQUEST;
	// 当url为/时，显示判断界面
	if (strlen(url) == 1)
		strcat(url, "judge.html");
	check_state = CHECK_STATE_HEADER;
	return NO_REQUEST;
}

// 解析http请求的一个头部信息
HttpConnection::HTTP_CODE HttpConnection::parse_headers(char *text)
{
	if (text[0] == '\0')
	{
		if (content_length != 0)
		{
			check_state = CHECK_STATE_CONTENT;
			return NO_REQUEST;
		}
		return GET_REQUEST;
	}
	else if (strncasecmp(text, "Connection:", 11) == 0)
	{
		text += 11;
		text += strspn(text, " \t");
		if (strcasecmp(text, "keep-alive") == 0)
		{
			linger = true;
		}
	}
	else if (strncasecmp(text, "Content-length:", 15) == 0)
	{
		text += 15;
		text += strspn(text, " \t");
		content_length = atol(text);
	}
	else if (strncasecmp(text, "Host:", 5) == 0)
	{
		text += 5;
		text += strspn(text, " \t");
		host = text;
	}
	else
	{
		// printf("oop!unknow header: %s\n",text);
		LOG_INFO("oop!unknow header: %s", text);
		Logger::get_instance()->flush();
	}
	return NO_REQUEST;
}

// 判断http请求是否被完整读入
HttpConnection::HTTP_CODE HttpConnection::parse_content(char *text)
{
	if (read_idx >= (content_length + checked_idx))
	{
		text[content_length] = '\0';
		// POST请求中最后为输入的用户名和密码
		m_string = text;
		return GET_REQUEST;
	}
	return NO_REQUEST;
}

//
HttpConnection::HTTP_CODE HttpConnection::process_read()
{
	LINE_STATUS line_status = LINE_OK;
	HTTP_CODE ret = NO_REQUEST;
	char *text = 0;

	while ((check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK))
	{
		text = get_line();
		start_line = checked_idx;
		LOG_INFO("%s", text);
		Logger::get_instance()->flush();
		switch (check_state)
		{
		case CHECK_STATE_REQUESTLINE:
		{
			ret = parse_request_line(text);
			if (ret == BAD_REQUEST)
				return BAD_REQUEST;
			break;
		}
		case CHECK_STATE_HEADER:
		{
			ret = parse_headers(text);
			if (ret == BAD_REQUEST)
				return BAD_REQUEST;
			else if (ret == GET_REQUEST)
			{
				return do_request();
			}
			break;
		}
		case CHECK_STATE_CONTENT:
		{
			ret = parse_content(text);
			if (ret == GET_REQUEST)
				return do_request();
			line_status = LINE_OPEN;
			break;
		}
		default:
			return INTERNAL_ERROR;
		}
	}
	return NO_REQUEST;
}

HttpConnection::HTTP_CODE HttpConnection::do_request()
{
	strcpy(real_file, doc_root);
	int len = strlen(doc_root);
	const char *p = strrchr(url, '/');

	// 处理cgi
	if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3'))
	{

		// 根据标志判断是登录检测还是注册检测
		char flag = url[1];

		char *m_url_real = (char *)malloc(sizeof(char) * 200);
		strcpy(m_url_real, "/");
		strcat(m_url_real, url + 2);
		strncpy(real_file + len, m_url_real, FILENAME_LEN - len - 1);
		free(m_url_real);

		// 将用户名和密码提取出来
		// user=123&passwd=123
		char name[100], password[100];
		int i;
		for (i = 5; m_string[i] != '&'; ++i)
			name[i - 5] = m_string[i];
		name[i - 5] = '\0';

		int j = 0;
		for (i = i + 10; m_string[i] != '\0'; ++i, ++j)
			password[j] = m_string[i];
		password[j] = '\0';

		// 同步线程登录校验
		if (*(p + 1) == '3')
		{
			// 如果是注册，先检测数据库中是否有重名的
			// 没有重名的，进行增加数据
			char *sql_insert = (char *)malloc(sizeof(char) * 200);
			strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
			strcat(sql_insert, "'");
			strcat(sql_insert, name);
			strcat(sql_insert, "', '");
			strcat(sql_insert, password);
			strcat(sql_insert, "')");

			if (users.find(name) == users.end())
			{

				m_lock.lock();
				int res = mysql_query(mysql, sql_insert);
				users.insert({name, password});
				m_lock.unlock();

				if (!res)
					strcpy(url, "/log.html");
				else
					strcpy(url, "/registerError.html");
			}
			else
				strcpy(url, "/registerError.html");
		}
		// 如果是登录，直接判断
		// 若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
		else if (*(p + 1) == '2')
		{
			if (users.find(name) != users.end() && users[name] == password)
				strcpy(url, "/welcome.html");
			else
				strcpy(url, "/logError.html");
		}
	}

	if (*(p + 1) == '0')
	{
		char *m_url_real = (char *)malloc(sizeof(char) * 200);
		strcpy(m_url_real, "/register.html");
		strncpy(real_file + len, m_url_real, strlen(m_url_real));

		free(m_url_real);
	}
	else if (*(p + 1) == '1')
	{
		char *m_url_real = (char *)malloc(sizeof(char) * 200);
		strcpy(m_url_real, "/log.html");
		strncpy(real_file + len, m_url_real, strlen(m_url_real));

		free(m_url_real);
	}
	else if (*(p + 1) == '5')
	{
		char *m_url_real = (char *)malloc(sizeof(char) * 200);
		strcpy(m_url_real, "/picture.html");
		strncpy(real_file + len, m_url_real, strlen(m_url_real));

		free(m_url_real);
	}
	else if (*(p + 1) == '6')
	{
		char *m_url_real = (char *)malloc(sizeof(char) * 200);
		strcpy(m_url_real, "/video.html");
		strncpy(real_file + len, m_url_real, strlen(m_url_real));

		free(m_url_real);
	}
	else if (*(p + 1) == '7')
	{
		char *m_url_real = (char *)malloc(sizeof(char) * 200);
		strcpy(m_url_real, "/fans.html");
		strncpy(real_file + len, m_url_real, strlen(m_url_real));

		free(m_url_real);
	}
	else
		strncpy(real_file + len, url, FILENAME_LEN - len - 1);

	if (stat(real_file, &file_stat) < 0)
		return NO_RESOURCE;
	if (!(file_stat.st_mode & S_IROTH))
		return FORBIDDEN_REQUEST;
	if (S_ISDIR(file_stat.st_mode))
		return BAD_REQUEST;
	int fd = open(real_file, O_RDONLY);
	file_address = (char *)mmap(0, file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	return FILE_REQUEST;
}
void HttpConnection::unmap()
{
	if (file_address)
	{
		munmap(file_address, file_stat.st_size);
		file_address = 0;
	}
}

bool HttpConnection::write()
{
	int temp = 0;

	if (bytes_to_send == 0)
	{
		modfd(epoll_fd, sockfd, EPOLLIN);
		init();
		return true;
	}

	while (1)
	{
		temp = writev(sockfd, iv, iv_count);

		if (temp < 0)
		{
			if (errno == EAGAIN)
			{
				modfd(epoll_fd, sockfd, EPOLLOUT);
				return true;
			}
			unmap();
			return false;
		}

		bytes_have_send += temp;
		bytes_to_send -= temp;
		if (bytes_have_send >= iv[0].iov_len)
		{
			iv[0].iov_len = 0;
			iv[1].iov_base = file_address + (bytes_have_send - write_idx);
			iv[1].iov_len = bytes_to_send;
		}
		else
		{
			iv[0].iov_base = write_buf + bytes_have_send;
			iv[0].iov_len = iv[0].iov_len - bytes_have_send;
		}

		if (bytes_to_send <= 0)
		{
			unmap();
			modfd(epoll_fd, sockfd, EPOLLIN);

			if (linger)
			{
				init();
				return true;
			}
			else
			{
				return false;
			}
		}
	}
}

bool HttpConnection::add_response(const char *format, ...)
{
	if (write_idx >= WRITE_BUFFER_SIZE)
		return false;
	va_list arg_list;
	va_start(arg_list, format);
	int len = vsnprintf(write_buf + write_idx, WRITE_BUFFER_SIZE - 1 - write_idx, format, arg_list);
	if (len >= (WRITE_BUFFER_SIZE - 1 - write_idx))
	{
		va_end(arg_list);
		return false;
	}
	write_idx += len;
	va_end(arg_list);
	LOG_INFO("request:%s", write_buf);
	Logger::get_instance()->flush();
	return true;
}
bool HttpConnection::add_status_line(int status, const char *title)
{
	return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}
bool HttpConnection::add_headers(int content_len)
{
	add_content_length(content_len);
	add_linger();
	add_blank_line();
}
bool HttpConnection::add_content_length(int content_len)
{
	return add_response("Content-Length:%d\r\n", content_len);
}
bool HttpConnection::add_content_type()
{
	return add_response("Content-Type:%s\r\n", "text/html");
}
bool HttpConnection::add_linger()
{
	return add_response("Connection:%s\r\n", (linger == true) ? "keep-alive" : "close");
}
bool HttpConnection::add_blank_line()
{
	return add_response("%s", "\r\n");
}
bool HttpConnection::add_content(const char *content)
{
	return add_response("%s", content);
}
bool HttpConnection::process_write(HTTP_CODE ret)
{
	switch (ret)
	{
	case INTERNAL_ERROR:
	{
		add_status_line(500, error_500_title);
		add_headers(strlen(error_500_form));
		if (!add_content(error_500_form))
			return false;
		break;
	}
	case BAD_REQUEST:
	{
		add_status_line(404, error_404_title);
		add_headers(strlen(error_404_form));
		if (!add_content(error_404_form))
			return false;
		break;
	}
	case FORBIDDEN_REQUEST:
	{
		add_status_line(403, error_403_title);
		add_headers(strlen(error_403_form));
		if (!add_content(error_403_form))
			return false;
		break;
	}
	case FILE_REQUEST:
	{
		add_status_line(200, ok_200_title);
		if (file_stat.st_size != 0)
		{
			add_headers(file_stat.st_size);
			iv[0].iov_base = write_buf;
			iv[0].iov_len = write_idx;
			iv[1].iov_base = file_address;
			iv[1].iov_len = file_stat.st_size;
			iv_count = 2;
			bytes_to_send = write_idx + file_stat.st_size;
			return true;
		}
		else
		{
			const char *ok_string = "<html><body></body></html>";
			add_headers(strlen(ok_string));
			if (!add_content(ok_string))
				return false;
		}
	}
	default:
		return false;
	}
	iv[0].iov_base = write_buf;
	iv[0].iov_len = write_idx;
	iv_count = 1;
	bytes_to_send = write_idx;
	return true;
}
void HttpConnection::process()
{
	HTTP_CODE read_ret = process_read();
	if (read_ret == NO_REQUEST)
	{
		modfd(epoll_fd, sockfd, EPOLLIN);
		return;
	}
	bool write_ret = process_write(read_ret);
	if (!write_ret)
	{
		close_conn();
	}
	modfd(epoll_fd, sockfd, EPOLLOUT);
}