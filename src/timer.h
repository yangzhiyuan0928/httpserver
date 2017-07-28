#ifndef TIMER_HEAP_H_
#define TIMER_HEAP_H_

#include <iostream>
#include <queue>
#include <functional>
#include <netinet/in.h>
#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include "debug.h"
#include "util.h"

using std::exception;
using std::priority_queue;

#define BUFFER_SIZE 64
#define TIMEOUT_DEFAULT 500

struct timer_node
{
    int64_t      expire;    //定时时间，单位为ms
    void (*cb_func)();  //超时回调
    friend bool operator < (const struct timer_node t1, const struct timer_node t2)
    {
        return t1.expire > t2.expire;
    }

    timer_node(int delay_ms, void (*func)())
    {
        int64_t current_msec = get_currentmsec();
        if(delay_ms < 0)
            delay_ms = 0;
        expire = current_msec + delay_ms;
        cb_func = func;
    }
};

class time_heap
{
private:
	priority_queue<timer_node> timer;  //优先队列封装成最小堆

public:
	time_heap() throw (std::exception) {}
	~time_heap() {}

public:
    bool isempty()
    {
        return (timer.size()) == 0;
    }

	void add_timer(struct timer_node node) throw (std::exception)
	{
		timer.push(node);
	}

    void del_timer()
	{
		if(isempty())
			return;
		timer.pop();
	}

	timer_node top_timer()
	{
		return timer.top();	//优先队列，队列首元素
	}

	int get_top_msec()
	{
        if(isempty())
            return -1;
        struct timer_node tmp = top_timer();
        int64_t current_msec = get_currentmsec();
        if(tmp.expire > current_msec)
            return tmp.expire - current_msec;
        return 0;
	}

	void tick()
	{
		struct timer_node tmp = top_timer(); //获取队首元素
		int64_t current_msec = get_currentmsec();  //ms
		while(timer.size() > 0)
		{
			if(timer.size() == 0)
				break;
			if(tmp.expire > current_msec)  //均没超时
				break;
			if( tmp.cb_func )
				tmp.cb_func();
			del_timer();
			tmp = top_timer();
		}
	}
};

#endif
