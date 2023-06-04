#include "timer.h"

TimerList::TimerList()
{
	head = nullptr;
	tail = nullptr;
}

TimerList::~TimerList()
{
	auto tmp = head;
	while (tmp)
	{
		head = tmp->next;
		delete tmp;
		tmp = head;
	}
}

void TimerList::add_timer(Timer *timer)
{
	if (!timer)
		return;
	if (!head)
	{
		head = tail = timer;
		return;
	}
	if (timer->expire < head->expire)
	{
		timer->next = head;
		head->prev = timer;
		head = timer;
		return;
	}
	add_timer(timer, head);
}

void TimerList::add_timer(Timer *timer, Timer *list_head)
{
	auto prev = list_head;
	auto tmp = prev->next;
	while (tmp)
	{
		if (timer->expire < tmp->expire)
		{
			prev->next = timer;
			timer->next = tmp;
			tmp->prev = timer;
			timer->prev = prev;
			break;
		}
		prev = tmp;
		tmp = tmp->next;
	}
	if (!tmp)
	{
		prev->next = timer;
		timer->prev = prev;
		timer->next = nullptr;
		tail = timer;
	}
}

void TimerList::adjust_timer(Timer *timer)
{
	if (!timer)
		return;
	auto tmp = timer->next;
	if (!tmp || (timer->expire < tmp->expire))
		return;
	if (timer == head)
	{
		head = head->next;
		head->prev = nullptr;
		timer->next = nullptr;
		add_timer(timer, head);
	}
	else
	{
		timer->prev->next = timer->next;
		timer->next->prev = timer->prev;
		add_timer(timer, timer->next);
	}
}

void TimerList::del_timer(Timer *timer)
{
	if (!timer)
		return;
	if ((timer == head) && (timer == tail))
	{
		delete timer;
		head = nullptr;
		tail = nullptr;
		return;
	}
	if (timer == head)
	{
		head = head->next;
		head->prev = nullptr;
		delete timer;
		return;
	}
	if (timer == tail)
	{
		tail = tail->prev;
		tail->next = nullptr;
		delete timer;
		return;
	}
	timer->prev->next = timer->next;
	timer->next->prev = timer->prev;
	delete timer;
}

void TimerList::tick()
{
	if (!head)
		return;
	auto cur = time(nullptr);
	auto tmp = head;
	while (tmp)
	{
		if (cur < tmp->expire)
			break;
		tmp->callback_func(tmp->user_data);
		head = tmp->next;
		if (head)
			head->prev = nullptr;
		delete tmp;
		tmp = head;
	}
}