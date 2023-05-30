#ifndef MYSQL_CONNECTION_POOL
#define MYSQL_CONNECTION_POOL

#include <cstdio>
#include <cstring>
#include <error.h>
#include <pthread.h>
#include <list>
#include <string>
#include <iostream>
#include <mysql/mysql.h>
#include "../lock/locker.hpp"
#include "../lock/sem.hpp"
#include "../log/logger.hpp"

class ConnectionPool
{
private:
	int max_connection;			  // 最大连接数
	int cur_connection;			  // 当前连接数
	int free_connection;		  // 空闲连接数
	Locker mutex;				  // 互斥锁
	std::list<MYSQL *> conn_list; // 连接池
	Sem reserve;

public:
	std::string url;   // 主机地址
	unsigned int port; // 数据库端口号
	std::string user;
	std::string password;
	std::string database_name;
	bool log_on; // 日志开关

private:
	ConnectionPool()
	{
		cur_connection = 0;
		free_connection = 0;
	}
	~ConnectionPool()
	{
		destroy_pool();
	}

public:
	// 获取数据库连接
	MYSQL *get_connection()
	{
		if (conn_list.empty())
			return nullptr;
		MYSQL *conn = nullptr;
		reserve.wait();
		mutex.lock();

		conn = conn_list.front();
		conn_list.pop_front();
		free_connection--;
		cur_connection++;

		mutex.unlock();
		return conn;
	}
	// 释放连接
	bool release_connection(MYSQL *conn)
	{
		if (!conn)
			return false;
		mutex.lock();

		conn_list.push_back(conn);
		free_connection++;
		cur_connection--;

		mutex.unlock();
		reserve.post();
		return true;
	}
	// 获取可用连接数量
	int get_free_connection()
	{
		int ret = 0;
		mutex.lock();
		ret = free_connection;
		mutex.unlock();
		return ret;
	}
	// 销毁所有连接
	void destroy_pool()
	{
		mutex.lock();
		while (!conn_list.empty())
		{
			auto conn = conn_list.front();
			mysql_close(conn);
			conn_list.pop_front();
		}
		cur_connection = 0;
		free_connection = 0;
		mutex.unlock();
	}

	static ConnectionPool *get_instance()
	{
		static ConnectionPool conn_pool;
		return &conn_pool;
	}
	// 初始化
	void init(std::string _url, std::string _user, std::string _password, std::string _database_name, int _port, int _max_connection, bool _log_on)
	{
		url = _url;
		port = _port;
		user = _user;
		password = _password;
		database_name = _database_name;
		log_on = _log_on;
		for (int i = 0; i < _max_connection; i++)
		{
			MYSQL *conn = nullptr;
			conn = mysql_init(conn);
			if (!conn)
			{
				LOG_ERROR("Mysql Init Error!");
				exit(1);
			}
			conn = mysql_real_connect(conn, url.c_str(), user.c_str(), password.c_str(), database_name.c_str(), port, nullptr, 0);
			if (!conn)
			{
				LOG_ERROR("Mysql Connection Error!");
				exit(1);
			}
			conn_list.push_back(conn);
			free_connection++;
		}
		reserve = Sem(free_connection);
		max_connection = free_connection;
	}
};

class ConnectionRAII
{
private:
	MYSQL *conn_RAII;
	ConnectionPool *pool_RAII;

public:
	ConnectionRAII(MYSQL **conn, ConnectionPool *conn_pool)
	{
		*conn = conn_pool->get_connection();
		conn_RAII = *conn;
		pool_RAII = conn_pool;
	}
	~ConnectionRAII()
	{
		pool_RAII->release_connection(conn_RAII);
	}
};

#endif