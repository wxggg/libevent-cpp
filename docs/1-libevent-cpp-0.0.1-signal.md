Libevent是一个事件通知库，其API提供了一种机制用于在文件描述符上发生特定事件或者超时之后执行回调函数，此外，libevent还支持因为信号或常规超时引起的回调操作。libevent 旨在替换事件驱动的网络f服务器中的事件循环，应用程序只需要调用 `event_dispatch` ，然后动态添加或删除事件，而不需要更改事件循环。
Libevent 是用C语言编写的高性能网路库，这里尝试使用C++进行封装其中的接口，目前主要基于 libevent-1.1b 版本的代码。本文在简介libevent基本架构的同时主要分析libevent中对于信号的处理方式。本文C++封装代码可在[libevent-cpp](https://github.com/sharixos/libevent-cpp) 找到。

## Libevent 基本架构
Libevent中最关键的数据类型就是事件`event`和事件管理器，事件管理器其实就是`event_bas`。

### 事件-event
事件event的部分成员包括文件描述符、事件类型、ev_flags等，其中文件描述符`ev_fd`的功能不仅仅只是文件描述符，有时候其也会作为信号的类型，比如将SIGINT作为fd传给事件，最终在操作信号的时候将`ev_fd` 作为信号类型传给操作信号的函数。对于事件类型`ev_events`，libevent中的事件包括超时事件、读、写、信号以及EV_PERSIST，比较难理解的就是持续化，一般来说任何一个挂起的事件被激活，激活的原因可能是超时到了，也可能是它的fd的读或者写准备好了，那么在回调函数执行之前这个事件会变为非挂起，如果想让它继续挂起就只能在回调函数中将事件再次添加。但是设置了EV_PERSIST 属性的事件在回调函数触发执行之后，其状态会刷新然后继续保持挂起状态，也就是再有超时到了或者读写满足了回调函数也会再次执行。如果需要让持久化的时间变为非挂起，则需要在回调函数中将该事件删除。
在`event`类中另外一个类型就是所处的列表类型，也就是 `ev_flags`，一共包括如：超时、插入、信号、激活、内部、初始化及全部几种类型，事件被创建的时候会将`ev_flags` 置为EVLIST_INIT,后面会根据事件的特性来添加不同的列表类型，比如信号事件在加入的时候会加入EVLIST_SIGNAL。
```c++
class event
{
  public:
	int ev_fd;
	short ev_events; /* EV_TIMEOUT EV_READ EV_WRITE EV_SIGNAL EV_PERSIST */
	short ev_ncalls;

	int ev_pri; /* smaller numbers are higher priority */
	event_base *ev_base;
	void (*ev_callback)(int, short, void *arg);

	/** EVLIST_TIMEOUT EVLIST_INSERTED EVLIST_SIGNAL EVLIST_ACTIVE
	 * EVLIST_INTERNAL EVLIST_INIT EVLIST_ALL */
	int ev_flags;

	struct timeval ev_timeout;
};
```
事件类中另一个比较重要的成员就是回调函数指针，回调函数指针在事件初始化的时候由用户来指定，需要注意的就是libevent中回调函数的参数固定的使用(int fd, short event, void *arg)，其中第一个参数int表示文件描述符，第二个参数表示回调函数的返回值，可以在回调函数中指定，最重要的就是最后一个参数`void *arg`，它实际上是传进回调函数的`event *`类型，也就是事件指针，能够访问对应事件的内容。

### 事件管理器-event_base
Libevent中的事件管理器`event_base`主要功能是对于事件进行管理，包括事件的添加和删除等操作，所以`event_base`中比较重要的就是保存事件的数据结构，在libevent-1.1b版本中使用了事件队列eventqueue、信号队列signalqueue以及多个激活队列activequeues，为什么要有多个激活队列呢，因为这样就可以区分不同激活队列之间的优先级，将优先级靠前的激活队列中的事件优先处理。一般来说准备要处理的事件都要加入到激活队列中依次处理，那么为什么还需要有signalqueue和eventqueue这两个结构呢，原因在于对于信号事件来说，在没有信号的时候信号事件并没有被激活，也就是没有信号触发的时候信号事件只能等待，不能进入激活队列进行处理，所以就使用signalqueue来保存信号事件，相应的eventqueue的目的是保存实时信号也就是realtime信号对应的事件，libevent使用了红黑树结构来保存超时事件。
```c++
class event_base
{
  public:
	int event_count;		/* counts of total events */
	int event_count_active; /* counts of active events */
	int event_gotterm;		/* Set to terminate loop */

	std::vector<std::list<event *>> activequeues;
	std::list<event *> eventqueue;
	std::list<event *> signalqueue;

	evsignal *evsig;

  public:
	virtual int add(event *) { return 0; }
	virtual int del(event *) { return 0; }
	virtual int recalc(int max) { return max; }
	virtual int dispatch(struct timeval *) { return 0; }

	int priority_init(int npriorities);
	int loop(int flags);

	int add_event(event *ev, struct timeval *tv);
	int del_event(event *ev);
	void event_active(event *ev, int res, short ncalls);

	void event_queue_remove(event *ev, int queue);
	void event_queue_insert(event *ev, int queue);

  private:
	void event_process_active();
};
```
简单来说事件管理器的功能就是管理事件，将未激活的事件和激活后的事件分别加入对应的数据结构进行保存。需要注意的就是这里加入的四个虚函数，add、del、recalc以及dispatch，libevent中将底层的网络接口封装成了一个统一的接口，这些底层接口包括select、poll、devpoll、epoll以及kqueue和win32接口等，这里的c++实现中使用类继承的方式让子类如`select_base`来实现这些方法以使用不同的底层接口，这些子类都会相应的初始化一个信号类型，这个信号类型用来处理程序执行过程中的信号事件，包括信号的阻塞和解阻塞等。那么上述的虚函数接口的功能到底是什么呢，以select来说，这四个函数接口对应事件的添加、删除以及计算和分发，添加和删除既包括对于select中的文件描述符集合的操作，也包括对于信号的添加和删除，而select中的计算`recalc`主要就是将Linux信号的信号处理函数与对应的信号类型进行初始化。分发操作dispatch则是对应的事件是否触发了，对于select来说，会阻塞在`select`函数这里，等待信号出现或者IO读写等是否满足，再相应的进行后续的处理。

## Libevent 信号处理及信号的生命周期
信号的处理对于一个Linux程序尤其是网络程序来说特别重要，因为Linux程序在执行的过程中会收到各种各样的信号，包括SIGINT、SIGTERM及SIGSTOP等，而网络接口如`select`在阻塞时也会因为收到各种信号而退出，libevent对于信号的处理是将其统一成libevent中的事件，将事件添加到激活事件队列中统一处理。对于libevent中的信号类型`evsignal`如下，其中`ev_signal`类型就是`event *`类型，最终会被添加到激活事件队列中，而私有类型`evsigmask`是信号处理都会需要的信号集合，这里主要用于信号的阻塞以及解除阻塞，其余的成员这里都设置为static了，因为在后续的信号处理函数中会调用，而信号处理函数作为函数指针需要为静态类型。这里的`caught`为`sig_atomic_t`类型用来保证信号处理和使用caught的代码对caught的访问能够原子的进行。在`evsigcaught`数组中记录了对应sig信号发生的次数，sig信号对应着就是信号事件中的fd，`needrecalc`成员用来确保`event *ev_signal`被且只被添加一次，ev_signal_pair是用来进行通信的文件描述符队，作为 `socketpair`的参数。
```c++
class evsignal
{
  private:
	sigset_t evsigmask;
  public:
	static short evsigcaught[NSIG];
	static volatile sig_atomic_t caught;
	static int ev_signal_pair[2];
	static int needrecalc;
	static int ev_signal_added;

	event *ev_signal;
  public:

	int add(event *ev);
	int del(event *ev);
	int recalc();
	int deliver();
	void process();

  private:
	/* Callback for when the signal handler write a byte to our signaling socket */
	static void handler(int sig);
	static void callback(int fd, short what, void *arg);
};
```
信号类中所有信号的信号处理函数都是handler，不同的是sig的不同，对于evsigcaught中的数据我们只关心我们注册的信号，也就是signalqueue中的信号事件对应的信号，前面提到ev_signal_pair，其中ev_signal_pair[1]作为ev_signal事件的fd，所以在handler函数中对ev_signal_pair[0]的写入会触发ev_signal的读，然后会调用ev_signal的callback函数。
```c++
void evsignal::callback(int fd, short what, void *arg)
{
	static char signals[100];
	event *ev = (event*)arg;
	int n = read(fd, signals, sizeof(signals));
	if (n == -1)
	{
		exit(-1);
	}
	ev->ev_base->add_event(ev, NULL);
}

void evsignal::handler(int sig)
{
	evsigcaught[sig]++;
	caught = 1;

	/* Wake up our notification mechanism */
	write(ev_signal_pair[0], "a", 1);
}
```

### 信号事件及事件管理器初始化
以信号事件的注册及处理为例查看libevent事件的生命周期，首先需要初始化`event` 及 `event_base` 类型，这里使用的是子类`select_base`，首先调用 `prirority_init` 初始化1个激活事件队列，然后初始化`event`，对应的`ev_fd`设置为 `SIGINT`，表示中断信号，同时设置相应的回调函数，在回调函数执行之后删除该事件。 在`select_base` 中加入该信号，`add_event`最终调用 `event_queue_insert` 之后会将中断信号事件加入到signalqueue队列中。
```c++
void signal_cb(int fd, short event, void *arg)
{
    cout<<"signal call back \n";
    eve::event *ev = (eve::event*) arg;
    ev->ev_base->del_event(ev);
}

int main(int argc, char const *argv[])
{
    eve::select_base sel;
    sel.priority_init(1); // allocate a single active queue

    eve::event signal_int;
    signal_int.set(&sel, SIGINT, EV_SIGNAL|EV_PERSIST, &signal_cb);

    sel.add_event(&signal_int, NULL);

    sel.loop(0);

    return 0;
}
```
值得注意的是，在信号事件被加入到signalqueue的同时，evsig的成员sigset_t类型的evsimask中会同样设置对应信号事件的ev_fd，也就是信号的类型，这里也就是SIGINT。具体就是调用`sigaddset`函数。
```c++
int event_base::add_event(event *ev, struct timeval *tv)
{
	...
	else if ((ev->ev_events & EV_SIGNAL) &&
			 !(ev->ev_flags & EVLIST_SIGNAL))
	{
		event_queue_insert(ev, EVLIST_SIGNAL);
		return this->add(ev);
	}

	return 0;
}

int select_base::add(event *ev)
{
    if (ev->ev_events & EV_SIGNAL)
        return evsig->add(ev);
	...
}

int evsignal::add(event *ev)
{
	if (ev->ev_events & (EV_READ | EV_WRITE))
		std::cerr << __func__ << "RW err\n";
	sigaddset(&this->evsigmask, ev->ev_fd);
	return 0;
}
```

### 隐藏的内部信号事件管理evsignal
在事件管理器`select_base`的初始化时会同时初始化一个内部的信号事件管理，其目的是将Linux的信号处理转化为libevent中的信号事件进行处理，同理如果底层使用`epoll`及`kqueue`等接口的时候同样也需要初始化一个evsignal对象，这个evsignal就是前面提到的evsignal类，成员`event * ev_signal`对应着libevent的事件，这个创建的`evsig`对象中的`ev_signal`事件会之后被加入到事件队列中，也就是eventqueue，注意就是`ev_signal`为一个读事件，所以其实最终是被加入到保存读写事件的eventqueue中的。
```c++
select_base::select_base()
{
		...
    evsig = new evsignal(this);
}

evsignal::evsignal(event_base *base)
{
	std::cout << __func__ << std::endl;
	sigemptyset(&evsigmask);
	/* 
	 * Our signal handler is going to write to one end of the socket
	 * pair to wake up our event loop.  The event loop then scans for
	 * signals that got delivered.
	 */
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, ev_signal_pair) == -1)
	{
		std::cout << __func__ << " socketpair error\n";
	}

	ev_signal = new event;
	ev_signal->set(base, ev_signal_pair[1], EV_READ, this->callback);
	ev_signal->ev_flags |= EVLIST_INTERNAL;

	this->ev_base = base;
}
```

### 主循环-loop
回到主程序现在执行到`event_base`管理器的`loop`循环，如果循环退出表示程序的退出，这里只展示其中与信号事件处理有关的部分。
```c++
int event_base::loop(int flags)
{
	/* Calculate the initial events that we are waiting for */
	if (this->recalc(0) == -1)
		return -1;

	int done = 0;
	struct timeval tv;
	while (!done)
	{
		...
	}
	return 0;
}
```

### 信号处理设置-recalc
在进入while循环之前会有一个`recalc`重新计算的过程，因为是虚函数所以会调用子类select_base对应的接口，select_base中没有需要计算的内容，但是需要计算之前提到的隐藏的evsig成员。
```c++
int select_base::recalc(int max)
{
    return evsig->recalc();
}

int evsignal::recalc()
{
	if (!ev_signal_added)
	{
		ev_signal_added = 1;
		this->ev_base->add_event(this->ev_signal, NULL);
	}

	if (this->ev_base->signalqueue.empty() && !needrecalc)
		return 0;
	needrecalc = 0;

	if (sigprocmask(SIG_BLOCK, &evsigmask, NULL) == -1)
		return -1;

	struct sigaction sa;
	/* Reinstall our signal handler. */
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = this->handler;
	sa.sa_mask = this->evsigmask;
	sa.sa_flags |= SA_RESTART;

	for (auto ev : this->ev_base->signalqueue)
	{
		if (sigaction(ev->ev_fd, &sa, NULL) == -1)
			return -1;
	}
	return 0;
}
```
这里evsignal中的重新计算的内容就比较清楚了，基本上就是调用Linux对于信号的接口，比如`sigprocmask`等操作，那么具体干了些什么呢，首先`ev_signal_added`的功能是判断成员`event * ev_signal`有没有被添加，如果没有就在现在加入到`event_base`中，之后判断一下信号队列是否为空以及是否需要重新计算，看情况直接返回。

这里的`sigprocmask`的参数给的是SIG_BLOCK，目的是让`evsigmask`中对应的文件描述符或者说信号被阻塞，之前提到SIGINT已经被添加到evsigmask中了，所以SIGINT类型的信号出现时会被阻塞，信号被阻塞之后会在内核中排队等待处理，并在程序不阻塞该信号的时候被获得，也就是相应的使用`sigprocmask`设置SIG_UNBLOCK参数。

接下来的一个操作就是设置真正的Linux信号处理函数，对于signalqueue中的每一个信号事件都将信号处理函数设置为handler函数。
```c++
void evsignal::handler(int sig)
{
	std::cout << __func__ << std::endl;
	evsigcaught[sig]++;
	caught = 1;

	/* Wake up our notification mechanism */
	write(ev_signal_pair[0], "a", 1);
}
```

### 事件分发-dispatch
在计算一次也就是调用recalc一次之后事件及信号处理函数基本上都设置好了，然后在回到主循环的while中，开始进行事件的分发，也就是dispatch操作。
```c++
int event_base::loop(int flags)
{
	...
	while (!done)
	{
		...
		if (this->dispatch(&tv) == -1) return -1;
		...
	}
	return 0;
}
```
#### 解除信号阻塞-deliver
从evsignal的deliver代码中可以看到现在解除evsigmask中的信号的阻塞，也就是内核中排队的信号现在会被程序收到并调用前面提到的handler处理函数。
```c++
int select_base::dispatch(struct timeval *tv)
{
	...
    if (evsig->deliver() == -1)
        return -1;
	...
}

int evsignal::deliver()
{
	if (this->ev_base->signalqueue.empty())
		return 0;

	return sigprocmask(SIG_UNBLOCK, &this->evsigmask, NULL);
}
```
#### 程序等待select函数及信号的发生
程序执行到select时会睡眠等待并等待读写事件发生，如果发生了读写事件表示要进行相应读写事件的处理，这里只考虑信号事件，也就是在睡眠的过程中收到了SIGINT事件，则select函数返回，返回值为-1。

```c++
int select_base::dispatch(struct timeval *tv)
{
    int res = select(event_fds, &event_readset_out[0], &event_writeset_out[0], NULL, tv);

    if (evsig->recalc() == -1)
        return -1;

    if (res == -1)
    {
        if (errno != EINTR)
            return -1;
        evsig->process();
        return 0;
    }
    else if (evsignal::caught)
        evsig->process();

}
void evsignal::handler(int sig)
{
	evsigcaught[sig]++;
	caught = 1;

	/* Wake up our notification mechanism */
	write(ev_signal_pair[0], "a", 1);
}

void evsignal::process()
{
	short ncalls;
	for (auto ev : this->ev_base->signalqueue)
	{
		ncalls = evsigcaught[ev->ev_fd];
		if (ncalls)
		{
			if (!(ev->ev_events & EV_PERSIST))
				this->ev_base->del_event(ev);
			this->ev_base->event_active(ev, EV_SIGNAL, ncalls);
		}
	}

	memset(this->evsigcaught, 0, sizeof(evsigcaught));
	caught = 0;
}
```
并且信号处理函数handler被调用，将`evsigcaught[SIGINT]++`，并且将`caught`设置为1，表示捕获到了信号，之后会调用`evsig->process`处理，主要就是将发生的信号事件加入到激活队列中，这里通过`evsigcaught[]`数组来判断某种信号是否获取了。

#### 再论内部读事件ev_signal
为什么这样一个读事件与信号处理有关系呢，或者说libevent中设置这么一个读事件的目的在哪儿呢。先看信号事件处理函数handler中，最后会对`ev_signal_pair[[0]`发送一个字符，而ev_signal读事件中的ev_fd刚好就是`ev_signal_pair[1]`，这两个文件描述符在开始初始化的时候是使用`socketpair`联系起来的，也就是说当信号发生调用handler的时候，写入字符的时候会导致`ev_signal->ev_fd`变得可读，这样`ev_signal`事件就会被激活，之后会调用ev_signal的回调函数callback。
```c++
void evsignal::callback(int fd, short what, void *arg)
{
	static char signals[100];
	event *ev = (event*)arg;
	int n = read(fd, signals, sizeof(signals));
	if (n == -1)
	{
		exit(-1);
	}
	ev->ev_base->add_event(ev, NULL);
}

void evsignal::handler(int sig)
{
	std::cout << __func__ << std::endl;
	evsigcaught[sig]++;
	caught = 1;

	/* Wake up our notification mechanism */
	write(ev_signal_pair[0], "a", 1);
}
```

### 处理激活事件
经过前面信号事件从signalqueue队列到激活队列的转变之后，激活队列中有事件需要处理，这时`event_count_active`为激活事件的数目，现在会调用 `event_count_active`函数来处理激活事件队列，最终会调用event类中设置的回调函数ev_callback，并根据ncalls来判断需要进行多少次调用。
```c++
int event_base::loop(int flags)
{
	...
	while (!done)
	{
		...
		if (this->event_count_active)
		{
			event_process_active();
			if (!this->event_count_active && (flags & EVLOOP_ONCE)) 
				done = 1;
		}
		else if (flags & EVLOOP_NONBLOCK) done = 1;

		if (this->recalc(0) == -1) return -1;
	}
	return 0;
}

void event_base::event_process_active()
{
	if (!this->event_count_active)
		return;

	std::list<event *> *pactiveq;
	for (auto &item : this->activequeues)
	{
		if (item.size() > 0)
		{
			pactiveq = &item;
			break;
		}
	}

	short ncalls;
	for (auto &ev : *pactiveq)
	{
		event_queue_remove(ev, EVLIST_ACTIVE);

		/* Allows deletes to work */
		ncalls = ev->ev_ncalls;
		ev->ev_pncalls = &ncalls;
		while (ncalls)
		{
			ncalls--;
			ev->ev_ncalls = ncalls;
			(*ev->ev_callback)((int)ev->ev_fd, ev->ev_res, ev);
		}
	}
}
```

## 小结
至此，关于信号事件在libevent中的完整的生命周期分析完成。整体上来看基本上libevent做的事情就是对不同类型的事件进行封装，比如对信号事件通过Linux信号处理函数`sigaction`等来做底层操作，而同时抽象出一个信号事件加入到libevent中相应的事件队列中，而对于IO事件则通过如select这样的底层接口进行抽象，最终都会整合到以优先级区分的多个激活队列，统一处理其中的事件，根据事件的调用次数来调用相应的回调函数。

* 本文所使用的代码可在 [libevent-cpp](https://github.com/sharixos/libevent-cpp) 找到
* 代码目前主要基于libevent1.1b，使用c++进行封装