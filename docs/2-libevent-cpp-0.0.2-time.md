上一篇介绍主要关于libevent的基本架构，并以C++的方式重写了其中的信号机制，在事件本身的结构还没有进行过多的抽象。在原本的libevent中，事件的类型是由 `ev_events` 来决定的，而事件所处的状态是由 `ev_flags` 来决定的。但是客观上来说，不同类型的事件之间的处理方式差别还是挺大的，因而可以分别以不同类进行抽象处理。这篇文章主要在进行事件抽象的基础上增加了时间处理，能够对超时事件进行响应。

## 重构事件的起源-event类
从重构的角度来看，对于信号事件、时间时间和读写事件应该分别进行抽象，并将尽可能相同的功能放到基类event中，所以就暂时有了如下的event：
```c++
class event
{
  public:
	event_base *_base;
	short _events; /* EV_TIMEOUT EV_READ EV_WRITE EV_SIGNAL EV_PERSIST */
	short _ncalls = 0;
	int _pri; /* smaller numbers means higher priority */

	void (*_callback)(short, void *arg);
	int _res; /* result passed to event callback */

  public:
	event(event_base *base);
	virtual ~event() { std::cout << __func__ << std::endl; }

	void set_base(event_base *base) { _base = base; }
	void set_callback(void (*callback)(short, void *)) { _callback = callback; }

	virtual void add() {}
	virtual void del() {}

	void activate(int res, short ncalls);
};
```
与上一个版本最大的区别在于，事件的添加和删除变成了事件自己的功能，这样的好处在于可以对于不同的事件进行不同的处理，并加入到对应的数据结构中。另外对于普通事件的激活也由事件类本身进行处理，而不是由之前的`event_base` 进行处理，虽然最终仍然是加入到`event_base` 的激活队列中，但是这样可以方便的由事件本身决定是否激活自己。

### 信号事件类-signal_event
从派生的角度来看，以信号事件为例，更多的只需要关注自身的加入和删除，因为信号事件的加入和删除同时伴随着对于信号类型及信号集的处理，所以便有了如下的`signal_event`类。
```c++
class signal_event : public event
{
  public:
    int _sig = -1;

  public:
    signal_event(event_base *base);
    ~signal_event() { std::cout << __func__ << std::endl; }

    void set_sig(int sig) { _sig = sig; }

    void add();
    void del();
};
```
对于任何一个派生的事件类来说都需要重写添加和删除操作，当然在这其中就可以有针对性的进行处理自己独有的操作。对于信号处理来说就是对信号集合 `evsigmask` 的处理，当需要添加事件的时候自然会需要将当前事件的信号类型 `_sig` 使用 `sigaddset` 加入到 `evsigmask` 中，同样在删除的时候要进行删除，并且还需要重置 `_sig` 类型事件的处理函数，重置为 `(struct sigaction*)SIG_DFL`。当然还需要分别从信号事件队列 `signalqueue`中对事件进行增删。
```c++
void signal_event::add()
{
	std::cout << __PRETTY_FUNCTION__ << std::endl;
    this->_base->signalqueue.push_back(this);
    sigaddset(&this->_base->evsigmask, _sig);
}

void signal_event::del()
{
	std::cout << __PRETTY_FUNCTION__ << std::endl;
    this->_base->signalqueue.remove(this);
    sigdelset(&this->_base->evsigmask, _sig);
    sigaction(_sig, (struct sigaction*)SIG_DFL, NULL);
}
```

## time_event-超时事件的生命周期
对于时间事件其实一般都是指的超时事件，在libevent中可以设置多个超时时钟，然后根据设置时钟的先后顺序先后进行处理，libevent主要是对于Linux（或其他平台）时间处理进行了封装，首先对于一个时间事件最重要的元素在于 `struct timeval _timeout` ，也就是在何时超时，对于是否超时的判断也是围绕着这一数据进行。
```c++
class time_event : public event
{

  public:
    struct timeval _timeout;

  public:
    time_event(event_base *base);
    ~time_event() { std::cout << __func__ << std::endl; }

    void set_timer(int nsec);
    struct timeval *timer_left();

    void add();
    void del();
};
```

### 创建及初始化超时事件
首先初始化一个超时事件，并设置对应的回调函数，这里我们希望设置一个回调函数，最开始设置5秒为时钟，当被调用之后每隔3秒再次调用，也就是在回调函数处理之后再次将当前事件加入。
```c++
void timeout_cb(short event, void *arg)
{
	time_event *timeout = (time_event *)arg;
	int newtime = time(NULL);

	cout << __func__ << ": called at " << newtime << endl;

	timeout->set_timer(3);
	timeout->add();
}

int main(int argc, char **argv)
{
	select_base base;
	base.priority_init(1);

	time_event timeout(&base);
	timeout.set_callback(timeout_cb);
	timeout.set_timer(5);
	timeout.add();

	base.loop(0);
}
```
初始化的过程其实就是设置时钟和回调函数的过程，可以看一下时钟的设置，其实就是要设置`_timeout`的值，就是在获取当前时间的基础上在`tv_sec`上加上对应时钟的数值就行了。
```c++
void time_event::set_timer(int nsec)
{
    struct timeval now, tv;
    gettimeofday(&now, NULL);

    timerclear(&tv);
    tv.tv_sec = nsec;

    timeradd(&now, &tv, &_timeout);
}
```

### 有序的超时事件
然后就可以将事件加入到事件管理器中了，参照libevent中对时间的管理，libevent中使用了红黑树RBTree来管理超时事件，红黑树的一个特点是有序，另一个特点是高效，对于时间事件来说，需要一个有序的结构根据时间的前后顺序来保存，而且希望能够高效的进行增删，因而红黑树是不错的选择。对于C++ stl库来说，使用`set` 其实就能够达到同样的效果，因为`set`的底层实现同样是红黑树，但是`set`有序的前提是提供一个比较的结构，也就是如下的`cmp_timeev`结构，使用Linux的`timercmp`来比较时间事件的`_timeout`成员，对应着`time_event`的比较，从而最终 `set` 中时间事件根据`timeout`来排序。

```c++
struct cmp_timeev
{
    bool operator()(time_event *const &lhs, time_event *const &rhs) const;
};

class event_base
{
    std::set<time_event *, cmp_timeev> timeevset;
};

bool cmp_timeev::operator()(time_event *const &lhs, time_event *const &rhs) const
{
	return timercmp(&lhs->_timeout, &rhs->_timeout, <);
}
```

### 主循环
主循环中现在主要包括如下部分，首先是判断各个数据结构中是否还有事件，如果全都不存在的话那么说明已经全部处理完成可以直接退出。否则会进入dispatch将事件分发下去处理，需要注意的就是对于时间事件来说，这里需要确定dipatch的执行方式，如果没有时间事件的话给`dispatch`传入的参数是NULL，也就是说没有设置超时参数。如果存在超时事件又该如何呢，需要判断超时集合`timeevset`中是否存在已经到时间的事件，判断的方式也比较简单，当最前面的事件没有超时那么就全部都没有超时，这时就应该继续分发事件，执行dispatch，并附带一个最前面的时间事件的超时间隔。
```c++
while (!done)
{
    /** 判断如果没有事件直接退出 **/
    //...

    int res;
    struct timeval now, off;
    gettimeofday(&now, NULL);
    if (timeevset.empty()) {
        /* no timeout event */
        res = this->dispatch(NULL);
    }
    else {
        time_event *timeev = *timeevset.begin();
        /** judge if all timeout > now, if not then 
         *  means have some active timeout event need to be 
         *  processed, so do not dispatch, because dispatch 
         *  will wait until the next timeout appear */
        if (timercmp(&(timeev->_timeout), &now, >)) {
            timersub(&(timeev->_timeout), &now, &off);
            /* attach timeout is off */
            res = this->dispatch(&off);
        }
    }
    /* 如果dispatch出错返回-1直接退出*/
    //...

    timeout_process();
    event_process_active();

}
```

### 超时的产生、激活及处理
我们现在实现的对底层IO的封装仍然还是`select`，所以仍以它为例，首先程序会等待在`select`函数，等待超时、信号以及输入输出事件，当时间间隔tv到了之后不再继续等待，回到主循环之后会执行时间处理和激活事件处理。
```c++
int select_base::dispatch(struct timeval *tv) {
    //...
    int res = select(event_fds, event_readset_out, event_writeset_out, NULL, tv);
    //...
}
```
对于时间的处理主要就是从前往后遍历时间事件集合，将其中小于当前时间的事件从集合中取出，并进行激活，激活后的事件会加入到激活事件队列中，然后在激活事件队列的遍历过程中会调用对应的回调函数进行处理。
```c++
void event_base::timeout_process()
{
	std::cout << __func__ << std::endl;
	struct timeval now;
	gettimeofday(&now, NULL);

	time_event *ev;
	std::set<time_event *, cmp_timeev>::iterator i = timeevset.begin();
	while (i != timeevset.end())
	{
		ev = *i;
		if (timercmp(&ev->_timeout, &now, >))
			break;
		i = timeevset.erase(i);
		ev->activate(EV_TIMEOUT, 1);
	}
}

void event_base::event_process_active()
{
    //选一个最高优先级且不空的激活队列activeq
	if (!activeq.empty())
	{
		event *ev;
		std::list<event *>::iterator i = activeq.begin();
		while (i != activeq.end())
		{
			ev = *i;
			while (ev->_ncalls)
			{
				ev->_ncalls--;
				(*ev->_callback)(ev->_res, ev);
			}
			i = activeq.erase(i);
		}
	}
}
```
从这里可以看到当回调函数执行之后，该事件会从激活队列中删除，考虑到我们当前的测试程序回调函数会将超时事件重新加入，并重新设置超时时间，因而该事件又会在主循环中再次进行轮转。

## 小结
超时事件主要就是要进行时间的判断，这里底层IO函数`select`实际上是真正的等待超时时间的点。
* [项目地址](https://github.com/wxggg/libevent-cpp)