#ifndef HTTP_CONN
#define HTTP_CONN

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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
#include <map>

#include "../lock/locker.hpp"
#include "../pool/mysql_connection_pool.hpp"
#include "../utils/timer.h"
#include "../log/logger.hpp"

class HttpConnection
{
public:
	static const int FILENAME_LEN = 200;
	static const int READ_BUFFER_SIZE = 2048;
	static const int WRITE_BUFFER_SIZE = 1024;
	enum METHOD
	{
		GET = 0,
		POST
	};
	// 状态机状态
	enum CHECK_STATE
	{
		CHECK_STATE_REQUESTLINE = 0,
		CHECK_STATE_HEADER,
		CHECK_STATE_CONTENT
	};
	// Http协议状态码
	enum HTTP_CODE
	{
		NO_REQUEST,
		GET_REQUEST,
		BAD_REQUEST,
		NO_RESOURCE,
		FORBIDDEN_REQUEST,
		FILE_REQUEST,
		INTERNAL_ERROR,
		CLOSED_CONNECTION
	};
	enum LINE_STATUS
	{
		LINE_OK = 0,
		LINE_BAD,
		LINE_OPEN
	};

public:
	HttpConnection() {}
	~HttpConnection() {}

public:
	void init(int sockfd, const sockaddr_in &addr);
	void close_conn(bool read_close = true);
	void process();
	bool read_once();
	bool write();
	sockaddr_in *get_address()
	{
		return &address;
	}
	void init_mysql_result(ConnectionPool *conn_pool);

private:
	void init();
	HTTP_CODE process_read();
	bool process_write(HTTP_CODE ret);
	HTTP_CODE parse_request_line(char *text);
	HTTP_CODE parse_headers(char *text);
	HTTP_CODE parse_content(char *text);
	HTTP_CODE do_request();
	char *get_line()
	{
		return read_buf + start_line;
	}
	LINE_STATUS parse_line();
	void unmap();
	bool add_response(const char *format, ...);
	bool add_content(const char *content);
	bool add_status_line(int status, const char *title);
	bool add_headers(int content_length);
	bool add_content_type();
	bool add_content_length(int content_length);
	bool add_linger();
	bool add_blank_line();

public:
	static int epoll_fd;
	static int user_count;
	MYSQL *mysql;

private:
	int sockfd;
	sockaddr_in address;
	char read_buf[READ_BUFFER_SIZE];
	int read_idx;
	int checked_idx;
	int start_line;
	char write_buf[WRITE_BUFFER_SIZE];
	int write_idx;
	CHECK_STATE check_state;
	METHOD method;
	char real_file[FILENAME_LEN];
	char *url;
	char *version;
	char *host;
	int content_length;
	bool linger;
	char *file_address;
	struct stat file_stat;
	struct iovec iv[2];
	int iv_count;
	int cgi;
	char *m_string;
	int bytes_to_send;
	int bytes_have_send;
	bool log_on;
};

#endif