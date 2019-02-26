关于libevent除了对于信号和超时的处理，最核心的部分其实是对于多路IO复用的封装。IO复用存在多种方式，包括select、poll、kqueue及epoll等多种接口。libevent的实现中用C语言以类似面向对象的方式来将这多种IO复用方式进行封装，对于我们的libevent-cpp而言，可以直接使用父子继承来实现各种不同的IO方式。本文主要介绍一下select机制以及libevent中使用select来处理IO读写事件的方式，然后深入Linux内核介绍一下select的实现机制。

## 读写事件管理及select机制
 

### select机制
如下为select函数及对应的参数，参数中最重要的数据类型就是fd_set数组对应的读描述符集合和写描述符集合，其中分别保存着读写描述符的信息，进程首先会睡眠于select函数，之后一旦读或者写描述符集合中的某个fd满足读或者写了，那么select就会返回，然后进程处理对应描述符的读写。
```c++
int select(int nfds, fd_set *readfds, fd_set *writefds,
                  fd_set *exceptfds, struct timeval *timeout);
```

### select_base 管理读写事件
之前提到对于信号事件和超时事件都有着对应的数据结构来保存加入的事件，信号事件是使用列表而超时事件为了满足有序和快速使用了set数据结构。对于select管理的读写事件而言，我们希望能够保存加入的读写事件，当然也希望能够有一个fd到读写事件rw_event的对应关系，因而使用map就比较方便，在libevent原始代码中是使用一个很大的数组来表示对应关系，并使用数组下表表示描述符。另外对于select函数的参数最好也能够统一管理，这样就有了如下的select_base读写事件管理类
```c++
class select_base : public event_base
{
  private:
	int event_fds = 0; /* Highest fd in fd set */
	int event_fdsz = 0;

	fd_set *event_readset_in = nullptr;
	fd_set *event_writeset_in = nullptr;

	std::map<int, rw_event *> fd_map_rw;

    //...
};
```
然后对于读写事件的添加和删除其实主要就是对如上数据结构内容的修改，以读写事件的添加为例对如上`event_readset_in`中设置读类型事件的文件描述符对应的内容为1，对于文件描述符和读写事件的映射中加入对应的数据即可。
```c++
int select_base::add(rw_event *ev)
{
    //...resize() 首先需要判断加入的事件文件描述符是否超过了管理
    //   的最大文件描述符，如果超过了就重新分配相关数据结构大小

    if (ev->is_readable()) //事件可读
    {
        FD_SET(ev->fd, event_readset_in);
        this->fd_map_rw[ev->fd] = ev;
    }
    if (ev->is_writable()) //事件可写
    {
        FD_SET(ev->fd, event_writeset_in);
        this->fd_map_rw[ev->fd] = ev;
    }
    return 0;
}
```
然后其实就是select函数的调用，主要调用过程是通过事件管理器的dispatch，只不过select_base重写了这个虚函数，因而实例化为select_base的event_base会调用select_base的diapatch函数，最终调用select，可以看一下是怎么处理产生IO事件的。
```c++
int select_base::dispatch(struct timeval *tv)
{
    memcpy(event_readset_out, event_readset_in, event_fdsz);
    memcpy(event_writeset_out, event_writeset_in, event_fdsz);

    //...

    int res = select(event_fds + 1, event_readset_out, event_writeset_out, nullptr, tv);

    //...

    bool iread, iwrite;
    rw_event *ev;

    for (auto kv : fd_map_rw)
    {
        iread = iwrite = false;
        if (FD_ISSET(kv.first, event_readset_out))
            iread = true;
        if (FD_ISSET(kv.first, event_writeset_out))
            iwrite = true;

        if ((iread || iwrite) && kv.second)
        {
            ev = kv.second;
            if (iread && ev->is_readable())
                ev->set_active_read();
            if (iwrite && ev->is_writable())
                ev->set_active_write();

            if (ev->is_read_active() || ev->is_write_active())
            {
                if (!ev->is_persistent())
                    ev->del();
                ev->activate(1);
            }
        }
    }

    return 0;
}
```
当select函数因为出现读写而返回时，其实是有些描述符满足了读或者写，在之后分别遍历保存文件描述符和对应读写事件的映射`fd_map_rw`，将其中出现读写的事件进行激活，并从映射中移出，这样之后进行激活事件处理的时候就能调用对应事件的回调函数进行相关读写事件的处理了。

## Linux内核select机制实现原理
就Linux而言，库函数`select`最终会通过系统调用进入内核态，然后由内核进行相关的处理，Linux内核中对于select的支持从Linux-2.0版本之后才开始加入，所以这里本文主要就Linux-2.0.1版本进行分析，主要代码位于内核`fs/select.c`中。

### select的内核实现-sys_select
如下的内核函数`sys_select`对应着用户态`select`函数的实现，可以看到参数类型完全相同，但是sys_select函数其实并没有真正的处理读写文件描述符集合，因为传入的指针如`fd_set *inp`是用户态的描述符集合数组，首先需要将用户态的数据复制到内核态的`in`，并且同时需要处理超时timeout，并设置进程的超时时间为timeout，然后调用`do_select`真正的进行处理相关描述符集合，处理完之后出现了读写导致某个描述符被修改，然后需要将得到的内核态结果如`fd_set *res_in`传回到用户态数据 `fd_set *inp`。
```c++
asmlinkage int sys_select(int n, fd_set *inp, fd_set *outp, fd_set *exp, struct timeval *tvp)
{
	int i;
	fd_set res_in, in;
	fd_set res_out, out;
	fd_set res_ex, ex;
	unsigned long timeout;

	//... 先判断n需要在一定的范围内

    /* 如下get_fd_set 用来将 inp 内容复制到 in
     * inp的内容为用户态的数据，而in是内核态 */
	if ((i = get_fd_set(n, inp, &in)) || ...)) return i;

	timeout = ~0UL;
	if (tvp) { timeout = ... // 处理超时时间 }
	current->timeout = timeout;

	i = do_select(n, &in, &out, &ex, &res_in, &res_out, &res_ex);

    //... 处理timeout

	if (i < 0) return i;
	if (!i && (current->signal & ~current->blocked))
		return -ERESTARTNOHAND;

    /* 使用set_fd_set将内核态的do_select处理之后的res_in
     * 复制回用户态数据区 inp */
	set_fd_set(n, inp, &res_in);
    ...
	return i;
}
```

### 轮询检查描述符集合-do_select
关于`do_select` 函数，主要其实就是轮询检查描述符集合中是否出现io操作，并分别进行处理。主要操作在如下for循环中，依次检查每个文件描述符，以及通过文件描述符检查对应的文件是否出现io操作，在此之前将进程的状态设置为TASK_INTERRUPTIBLE，表示当前进程可以被信号和wakeup唤醒。轮询判断之后，如果没有io操作且没有超时且出现的信号不是非阻塞的就继续调度。
#### 关于TASK_INTERRUPTIBLE状态
进程处于TASK_INTERRUPTIBLE状态意味着该进程会从就绪进程队列中被移除，也就是说在以后的调度函数`schedule`执行时，处于TASK_INTERRUPTIBLE状态的进程不会被调度，这样的目的是让`select`函数不会处于忙等待的状态，也就是没有io操作或超时或信号等的时候不会浪费cpu一直查询。而当前进程处于TASK_INTERRUPTIBLE状态时，只有两种方式会被唤醒，一种是出现了中断或者说有针对当前进程的信号出现，另外一种是当前进程被`wakeup`了，实际上是通过`wakeup`来唤醒当前进程，这就涉及到Linux对于文件的处理，与检查函数`check`有关。
```c++
static int do_select(int n, fd_set *in, fd_set *out, fd_set *ex,
	fd_set *res_in, fd_set *res_out, fd_set *res_ex)
{
	int count;
	select_table wait_table, *wait;
	struct select_table_entry *entry;
	unsigned long set;
	int i,j;
	int max = -1;

	//... 文件描述符及文件检查
	
    n = max + 1;
	if(!(entry = (struct select_table_entry*) __get_free_page(GFP_KERNEL)))
		return -ENOMEM;
	FD_ZERO(res_in); //...清零res_in res_out res_ex
	count = 0;
	wait_table.nr = 0;
	wait_table.entry = entry;
	wait = &wait_table;

repeat:
	current->state = TASK_INTERRUPTIBLE; // TASK_INTERRUPTIBLE 能被信号和wakeup唤醒
	for (i = 0 ; i < n ; i++) {
        // 依次查询该文件描述符是否出现io操作，check调用底层驱动函数
		if (FD_ISSET(i,in) && check(SEL_IN,wait,current->files->fd[i])) {
			FD_SET(i, res_in);
			count++;
			wait = nullptr;
		}
		//... out ex
	}
	wait = nullptr;
    // 如果没有io操作 且 没有超时 且 出现的信号不是非阻塞的
	if (!count && current->timeout && !(current->signal & ~current->blocked)) {
		schedule(); // 重新调度，主动释放cpu
		goto repeat;
	}
	free_wait(&wait_table);
	free_page((unsigned long) entry);
	current->state = TASK_RUNNING; // 设置进程状态能被调度执行
	return count;
}
```

### Linux文件底层操作及select检查-check
首先对于文件而言，其接口如`read`、`write`及 `select`等操作都与驱动程序有关，对于不同的读写设备而言，可能需要不同的接口实现，当然，最终不同的设备在Linux中都被统一为文件操作，就是通过文件操作接口抽象来进行的。这里的check函数会调用底层的`select`函数来判断文件是否准备好了，如果准备好了后面进程就可以进行相关处理，如果没有准备好的话，当前进程会被加到这个文件的等待队列中，以后一旦这个文件准备好了，内核就会唤醒这个状态为TASK_INTERRUPTIBLE的进程，然后继续上述的`do_select`后面的操作，包括释放等待列表，并将状态重新设置为可调度状态也就是TASK_RUNNING。
```c++
/*
 * The check function checks the ready status of a file using the vfs layer.
 *
 * If the file was not ready we were added to its wait queue.  But in
 * case it became ready just after the check and just before it called
 * select_wait, we call it again, knowing we are already on its
 * wait queue this time.  The second call is not necessary if the
 * select_table is nullptr indicating an earlier file check was ready
 * and we aren't going to sleep on the select_table.  -- jrs
 */

static int check(int flag, select_table * wait, struct file * file)
{
	struct inode * inode;
	struct file_operations *fops;
	int (*select) (struct inode *, struct file *, int, select_table *);

	inode = file->f_inode;
	if ((fops = file->f_op) && (select = fops->select))
		return select(inode, file, flag, wait)
		    || (wait && select(inode, file, flag, nullptr));
	if (flag != SEL_EX)
		return 1;
	return 0;
}

struct file_operations {
	int (*lseek) (struct inode *, struct file *, off_t, int);
	int (*read) (struct inode *, struct file *, char *, int);
	int (*write) (struct inode *, struct file *, const char *, int);
	int (*readdir) (struct inode *, struct file *, void *, filldir_t);
	int (*select) (struct inode *, struct file *, int, select_table *);
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

## 小结
* 文本主要介绍libevent对底层io复用接口之一select的封装
* 对于Linux内核中select的实现机制介绍主要基于linux-2.0.1的代码，更新的Linux可能有所不同