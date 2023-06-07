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
};

#endif