随着Linux的更新在linux-2.1.23版本中加入了poll机制，因为poll的加入导致select的实现变得基于poll了，本文在上一个版本的基础上在libevent-cpp中加入了对poll机制的封装。总的来说，主要介绍libevent-cpp poll机制以及Linux内核对poll机制的实现原理，另外还会介绍到内核中select实现因为poll而做的改变。

## 读写事件管理及poll机制

### poll机制
如下为`poll`库函数，接收的数据`struct pollfd *fds` 其实是一个`strucct pollfd` 类型的数组，`nfds`对应着数组中元素的个数，最后一个为超时。与select最大的区别在于将关注的文件描述符以及对应的事件都抽取出来整合成`struct pollfd`类型了，而select需要一个很大的文件描述符集合数组，而其中可能只有很少的几个描述符是我们关注的可能会有读写事件。所以从功能上来说`poll`其实是更高效的。
```c++
int poll(struct pollfd *fds, nfds_t nfds, int timeout);

struct pollfd {
    int   fd;         /* file descriptor */
    short events;     /* requested events */
    short revents;    /* returned events */
};
```

### poll_base 管理读写事件
首先对于读写事件的管理，仍然采用select中用到的文件描述符与`rw_event *`的映射关系fd_map_rw，这次将它抽取出来放入`event_base`中，因为对于底层IO的封装子类都免不了需要文件描述符与事件的映射关系，另外poll机制中独有的数据结构就是`fd_map_poll`，这是文件描述符与`struct pollfd *`之间的映射。

```c++
class poll_base : public event_base
{
  private:
    std::map<int, struct pollfd *> fd_map_poll;
  public:
    // add del dispatch recalc
};

class event_base
{
  public:
	std::map<int, rw_event *> fd_map_rw;
	int _fds = 0; /* highest fd of added rw_event */
	int _fdsz = 0;
    // ...
};
```

首先对于 poll_base中读写事件的添加和删除，主要其实就是对两个映射内容的修改，首先读写事件加入的时候，需要new一个新的`struct pollfd`，并设置对应的成员，主要是文件描述符和事件类型events，事件类型需要根据`rw_event *ev`是否可读或可写来进行设置。在删除的时候同样是需要将两个映射结构中的相关内容进行删除，并释放之前new的`struct pollfd`的内容。
```c++
int poll_base::add(rw_event *ev)
{
    fd_map_rw[ev->fd] = ev;
    struct pollfd *pfd = new struct pollfd;
    pfd->fd = ev->fd;
    pfd->events = 0;
    pfd->revents = 0;
    fd_map_poll[ev->fd] = pfd;
    if (ev->is_readable())
        pfd->events |= POLLIN;
    if (ev->is_writable())
        pfd->events |= POLLOUT;
    return 0;
}
int poll_base::del(rw_event *ev)
{
    delete fd_map_poll[ev->fd];
    fd_map_poll.erase(ev->fd);
    fd_map_rw.erase(ev->fd);
    return 0;
}
```
然后就是`dispatch`的处理，首先针对传入的超时时间结构获取对应的微秒数之后作为poll接口的超时参数，另外还需要手动将`fd_map_poll`中的`struct pollfd` 数据抽取出来组成数组，然后作为poll的参数，nfds就是fd_map_poll的size。在返回之后，如果某个`struct pollfd`事件有改变，直接查看其`revents`成员就可以知道发生了什么事情，poll机制中一些关键的事件包括POLLHUP、POLLERR、POLLIN及POLLOUT，根据对应发生的事情就可以设置激活读写事是否可读或可写，然后激活在激活队列中进行处理。
```c++
int poll_base::dispatch(struct timeval *tv)
{
    // ...
    int sec = -1;
    if (tv)
        sec = tv->tv_sec * 1000 + (tv->tv_usec + 999) / 1000;

    int nfds = fd_map_poll.size();
    struct pollfd fds[nfds];
    int i = 0;
    for (const auto kv : fd_map_poll)
        fds[i++] = *kv.second;

    int res = poll(fds, nfds, sec);

    // ... 根据poll结果res判断是否处理超时、信号返回还是继续处理读写事件

    int what = 0;
    rw_event *ev;
    for (i = 0; i < nfds; i++)
    {
        what = fds[i].revents;
        ev = fd_map_rw[fds[i].fd];
        if (what && ev)
        {
            /* if the file gets closed notify */
            if (what & (POLLHUP | POLLERR))
                what |= POLLIN | POLLOUT;
            if ((what & POLLIN) && ev->is_readable())
                ev->set_active_read();
            if ((what & POLLOUT) && ev->is_writable())
                ev->set_active_write();

            if (ev->is_read_active() || ev->is_write_active())
            {
                if (!ev->is_persistent())
                    ev->del();
                ev->activate(1);
            }
        }
    }
}
```

## Linux内核poll机制实现原理
用户态的`poll`库函数通过系统调用转换到内核态对应着`fs/select.c`中的sys_poll函数，如下内核代码基于linux-2.1.23版本，从这一版本开始linux加入了poll机制。

### poll内核调用-sys_poll
与之前提到的`sys_select`类似的是，sys_poll函数一开始做的也是将传入的用户太的数据如`struct pollfd`数组先通过`copy_from_user`函数给复制到内核区域，并给等待列表wait_table分配对应的内存，在处理好相关参数之后调用`do_poll`来真正执行poll操作。
```c++
asmlinkage int sys_poll(struct pollfd * ufds, unsigned int nfds, int timeout)
{
    int i, count, fdcount, err = -EINVAL;
	struct pollfd * fds, *fds1;
	poll_table wait_table;
	struct poll_table_entry *entry;

	lock_kernel(); // 从有些版本起，Linux内核允许抢占，所以需要加锁

	entry = (struct poll_table_entry *) __get_free_page(GFP_KERNEL);

	fds = (struct pollfd *) kmalloc(nfds*sizeof(struct pollfd), GFP_KERNEL);
	if (copy_from_user(fds, ufds, nfds*sizeof(struct pollfd))) {
		free_page((unsigned long)entry);
		kfree(fds);
		goto out;
	}

	// 处理timeout
	current->timeout = timeout;

	count = 0;
	wait_table.nr = 0;
	wait_table.entry = entry;

	fdcount = do_poll(nfds, fds, &wait_table); // 调用 do_poll

	current->timeout = 0;
	free_wait(&wait_table);
	free_page((unsigned long) entry);

	/* 将poll结果中的返回事件revents传回用户区 */
	fds1 = fds;
	for(i=0; i < (int)nfds; i++, ufds++, fds++) {
		__put_user(fds->revents, &ufds->revents);
	}
	kfree(fds1);
    // 如果poll中没有发生任何读写等操作 并且 产生了非阻塞的信号
	if (!fdcount && (current->signal & ~current->blocked))
		err = -EINTR;
	else
		err = fdcount;
out:
	unlock_kernel();
	return err;
}
```

### poll实现-do_poll函数
如下为内核中的`do_poll`函数，从原理上与`do_select`其实并无二样，都是采用轮询的方式遍历文件描述符，只不过如下do_poll遍历 `struct pollfd`数组时只需要遍历可能发生事件的数组，而do_select则不一样，其实现上会遍历一个完整的描述符数组，即使这个数组里面的绝大多数描述符都没有注册事件。所以从效率上来说poll是比select要好的。
```c++
static int do_poll(unsigned int nfds, struct pollfd *fds, poll_table *wait)
{
	int count;
	struct file ** fd = current->files->fd;

	count = 0;
	for (;;) {
		unsigned int j;
		struct pollfd * fdpnt;

		current->state = TASK_INTERRUPTIBLE;
        /* 遍历fds数组，调用相应文件对应的底层驱动poll 函数获得mask */
		for (fdpnt = fds, j = 0; j < nfds; j++, fdpnt++) {
			unsigned int i;
			unsigned int mask;
			struct file * file; //fd对应的文件

			mask = POLLNVAL;
			i = fdpnt->fd;
			if (i < NR_OPEN && (file = fd[i]) != NULL) {
				mask = DEFAULT_POLLMASK;
				if (file->f_op && file->f_op->poll)
					mask = file->f_op->poll(file, wait); //调用底层驱动的poll
				mask &= fdpnt->events | POLLERR | POLLHUP;
			}
			if (mask) { // mask非0，代表某些事件发生了
				wait = NULL;
				count++;
			}
			fdpnt->revents = mask;
		}

		wait = NULL;
        // 如果发生了某些读写等事件 或 已经超时 或 出现了非阻塞的信号 则跳出循环
		if (count || !current->timeout || (current->signal & ~current->blocked))
			break;
		schedule(); // 重新调度，释放cpu
	}
	current->state = TASK_RUNNING;
	return count;
}
```
如上代码中，循环遍历fds中执行的关键操作还是调用底层的驱动函数poll，它会返回该文件描述符对应的文件是否有事件发生。之后判断整个数组是否有事件发生，如果有事件发生或者已经超时或者出现了非阻塞的信号，那么就跳出循环不再继续等待，否则调用`schedule`进行重新调度，并释放cpu。并且当前进程已经被设置为可中断状态TASK_INTERRUPTIBLE，这个进程设置为某些文件的等待队列，当这些文件出现了读写事件或者超时或者出现了信号时，这个进程会被唤醒，继续轮询。

## 加入poll机制后select实现的变化
在加入了poll机制后，linux内核中对于底层驱动的接口事实上舍弃了原先的`select`底层函数，如下为linux-2.1.23版本的`struct file_operations`结构，其中对于文件的操作由`poll`替代了`select`，这么做也是有一定道理的，poll的可扩展性比select是要好的，而两者在原理上其实类似，都是采用轮询描述符的方式，只不过poll直接轮询的是注册事件的描述符，而select轮询的是从0到最大注册描述符之间的所有描述符。
```c++
struct file_operations {
	long long (*llseek) (struct inode *, struct file *, long long, int);
	long (*read) (struct inode *, struct file *, char *, unsigned long);
	long (*write) (struct inode *, struct file *, const char *, unsigned long);
	int (*readdir) (struct inode *, struct file *, void *, filldir_t);
	unsigned int (*poll) (struct file *, poll_table *);
	int (*ioctl) (struct inode *, struct file *, unsigned int, unsigned long);
	int (*mmap) (struct inode *, struct file *, struct vm_area_struct *);
	int (*open) (struct inode *, struct file *);
	void (*release) (struct inode *, struct file *);
	int (*fsync) (struct inode *, struct file *);
	int (*fasync) (struct inode *, struct file *, int);
	int (*check_media_change) (kdev_t dev);
	int (*revalidate) (kdev_t dev);
};
```

### 基于poll的do_select实现
如下为基于poll文件操作的`do_select`的实现，唯一变化的就是在调用底层函数时调用的是poll，先根据设置过的文件描述符获取对应的文件结构，然后根据文件调用底层poll，后面根据获得的mask来分析是否退出就和上述的`do_poll`如出一辙了。
```c++
static int do_select(int n, fd_set_buffer *fds)
{
    // ...参数处理
	wait_table.nr = 0;
	wait_table.entry = entry;
	wait = &wait_table;
	for (;;) {
		struct file ** fd = current->files->fd;
		current->state = TASK_INTERRUPTIBLE;
		for (i = 0 ; i < n ; i++,fd++) {
			unsigned long bit = BIT(i);
			unsigned long *in = MEM(i,fds->in);

			if (bit & BITS(in)) {
				struct file * file = *fd;
				unsigned int mask = POLLNVAL;
				if (file) {
					mask = DEFAULT_POLLMASK;
					if (file->f_op && file->f_op->poll)
						mask = file->f_op->poll(file, wait); // 调用底层poll
				}
				if ((mask & POLLIN_SET) && ISSET(bit, __IN(in))) {
					SET(bit, __RES_IN(in));
					retval++;
					wait = NULL;
				}
                // ... out ex
			}
		}
		wait = NULL;
		if (retval || !current->timeout || (current->signal & ~current->blocked))
			break;
		schedule();
	}
	free_wait(&wait_table);
	free_page((unsigned long) entry);
	current->state = TASK_RUNNING;
out:
	return retval;
}
```

## 基于poll操作的针对文件资源的进程睡眠及唤醒操作
前面提到无论是select机制还是poll机制在系统调用之后，内核之中都是通过轮询文件描述符集合的方式来判断该文件是否有读写。而最底层要执行的是文件的poll函数，最初的select机制采用的是select函数，后来都由poll函数替代。而poll函数会随着文件类型的不同在内核中文件poll的实现也是不同的，比如以pipe文件和fifo文件为例，二者在内核中属于不同类型的文件，有着不同类型的文件操作函数。那么poll机制是如何与Linux进程的睡眠和唤醒相结合的呢。

### 以管道pipe为例的pipe_poll函数
如下为内核中的fifo_poll函数实现，如果我们是使用mkfifo创建fifo文件的话，最终poll函数调用会调用如下函数，而其中最关键的就是这个`poll_wait`函数，其功能是将当前进程current加入到对应文件节点资源的等待列表`struct wait_queue`中去，这是一个与资源相关的等待队列，可以通过文件指针获取其首节点，而`poll_wait`将当前进程加入到等待队列之后，之后就可以通过该文件的各种操作来唤醒这个加入等待队列的进程，前面的`do_select`或`do_poll`循环跳出之后可以看到另一个函数`free_wait`，这是与`poll_wait`相对应的另一个用来将进程从等待队列中去除的操作。
```c++
static unsigned int pipe_poll(struct file * filp, poll_table * wait)
{
	unsigned int mask;
	struct inode * inode = filp->f_inode;

	poll_wait(&PIPE_WAIT(*inode), wait);
	mask = POLLIN | POLLRDNORM;
	if (PIPE_EMPTY(*inode))
		mask = POLLOUT | POLLWRNORM;
	if (!PIPE_WRITERS(*inode))
		mask |= POLLHUP;
	if (!PIPE_READERS(*inode))
		mask |= POLLERR;
	return mask;
}

void poll_wait(struct wait_queue ** wait_address, poll_table * p)
{
	struct poll_table_entry * entry;

	if (!p || !wait_address)
		return;
	if (p->nr >= __MAX_POLL_TABLE_ENTRIES)
		return;
 	entry = p->entry + p->nr;
	entry->wait_address = wait_address;
	entry->wait.task = current;
	entry->wait.next = NULL;
	add_wait_queue(wait_address,&entry->wait);
	p->nr++;
}
```
值得一提的是，`poll_wait`函数在几乎所有的文件操作函数`xxx_poll`中都会被调用，因为只有这样才会真正的将当前进程加入关联该文件的等待队列中。

### 等待的进程如何唤醒
前面提到在进程指针通过`poll_wait`被保存到等待队列中后，当前进程状态会被置于TASK_INTERRUPTIBLE的状态，也就是不接受调度，但是可以接收中断和唤醒。如果相关联的文件资源有其它的操作，如其它进程对pipe文件进行写入，那么等待队列中的所有的进程都会被唤醒。
如下，在对fifo文件读或者写的时候有可能被睡眠，也有可能唤醒其它已经睡眠的进程，关键看读写的状态，如果读的时候发现管道是空的，那么就会将当前进程置于睡眠状态，如果成功从pipe缓冲pipebuf读取到了数据到用户缓存buf，之后就会唤醒因该管道文件而睡眠的等待队列上的进程，通过PIPE_WAIT(*inode)获取该队列，并通过wake_up_interruptible进行唤醒。同理写操作也会进行类似的睡眠及唤醒操作，写操作成功会唤醒因没有数据读如而睡眠的读进程。

```c++
static long pipe_read(struct inode * inode, struct file * filp,
	char * buf, unsigned long count)
{
	...
	if (filp->f_flags & O_NONBLOCK) {
		...
	} else while (PIPE_EMPTY(*inode) || PIPE_LOCK(*inode)) {
		...
		if (current->signal & ~current->blocked)
			return -ERESTARTSYS;
		interruptible_sleep_on(&PIPE_WAIT(*inode));
	}
	while (count>0 && (size = PIPE_SIZE(*inode))) {
		// 从pipe缓冲pipebuf读取数据到用户程序缓存buf
		copy_to_user(buf, pipebuf, chars );
	}
	PIPE_LOCK(*inode)--;
	wake_up_interruptible(&PIPE_WAIT(*inode));
	...
	return 0;
}
	
static long pipe_write(struct inode * inode, struct file * filp,
	const char * buf, unsigned long count)
{
	...
	if (!PIPE_READERS(*inode)) { /* no readers */
		send_sig(SIGPIPE,current,0);
		return -EPIPE;
	}
	...
	while (count>0) {
		while ((PIPE_FREE(*inode) < free) || PIPE_LOCK(*inode)) {
			if (!PIPE_READERS(*inode)) { /* no readers */
				send_sig(SIGPIPE,current,0);
				return written? :-EPIPE;
			}
			if (current->signal & ~current->blocked)
				return written? :-ERESTARTSYS;
			if (filp->f_flags & O_NONBLOCK)
				return written? :-EAGAIN;
			interruptible_sleep_on(&PIPE_WAIT(*inode));
		}
		PIPE_LOCK(*inode)++;
		while (count>0 && (free = PIPE_FREE(*inode))) {
			// 从用户buf写入数据到pipebuf
			copy_from_user(pipebuf, buf, chars );
		}
		PIPE_LOCK(*inode)--;
		wake_up_interruptible(&PIPE_WAIT(*inode));
	}
	return written;
}
```

## 小结
* 1. libevent-cpp 中对于poll的封装
* 2. Linux内核poll及select代码分析基于linux-2.1.23版本
* 3. Linux中与poll及文件读写有关的进程睡眠及唤醒机制