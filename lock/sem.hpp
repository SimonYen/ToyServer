#ifndef SEM_HPP
#define SEM_HPP

#include <exception>
#include <semaphore.h>

/*
该文件实现了对信号量的封装
*/

class Sem
{
private:
	sem_t sem;

public:
	// 构造函数，初始化信号量
	Sem()
	{
		/*
			sem_init:
				第二个参数为0代表该信号量是当前进程的局部信号量，
				否则会多进程之间共享这个信号量。

				第三个参数代表信号量的初始值。
		*/
		if (sem_init(&sem, 0, 0) != 0)
			throw std::exception();
	}
	Sem(int num)
	{
		if (sem_init(&sem, 0, num) != 0)
			throw std::exception();
	}
	// 销毁信号量
	~Sem()
	{
		sem_destroy(&sem);
	}
	// 等待信号量
	bool wait()
	{
		// 以原子操作的方式将信号量减一，如果信号量为0则会阻塞
		return sem_wait(&sem) == 0;
	}
	// 增加信号量
	bool post()
	{
		return sem_post(&sem) == 0;
	}
};

#endif