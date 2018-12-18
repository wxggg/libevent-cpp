前面介绍了poll和select在内核中的实现原理，可以说本质上两者都是相同的，都是将用户空间的文件描述符及对应事件复制到内核空间，然后在内核空间轮询，如果轮询没有事件，则将当前进程置于该文件描述符的等待队列，如此，在之后该文件进行读写操作时就可以唤醒其等待队列，然后再次轮询时会发现发生了读或者写事件。但是轮询机制在大规模IO复用时会有着明显的缺陷，首先因为poll和select要不断的轮询，所以其时间复杂度为O(n)，随着文件描述符的增多会越来越慢，另外文件描述符的增多也会导致要从用户态拷贝到内核态的数据越来越多，在大规模IO复用中，这种拷贝多次执行显然是难以忍受的，可以说基于poll种种缺点，在Linux-2.6版本左右加入了epoll机制，能够非常高高效的进行IO复用。

首先针对IO复用这一机制，从原理上来说其实主要的几个思路是不会改变的。首先是用户程序需要将其关注的文件描述符或者相应的读写事件传递给内核，不做处理的话这里存在从用户态到内核态的拷贝，select和poll就是这么做的，其次比较关键的其实就是内核在获取了相应的文件描述符及事件之后，如何有针对性的监控该文件描述符是否发生事件，并根据有无来进行当前进程睡眠和唤醒，因为如果没有发生事件的话是需要释放cpu资源的。在引入epoll之前的Linux版本中，或者说在2.6以前的版本中，poll对进程的睡眠是通过一个`poll_wait`函数实现的，只要是实现了poll函数的虚拟文件系统，都会在`poll`中调用`poll_wait`，poll会将当前进程置于该文件的等待队列，然后在该虚拟文件系统的读写接口中会相应的进行等待队列进程的唤醒。

但是在引入了epoll机制之后，这种进程睡眠和唤醒机制是被改变了，或者说`poll_wait`函数也被改变了，epoll的思路是所有的与文件描述符及事件有关的数据都由内核来管理，提供一定的接口给用户程序进行相应事件的添加删除和修改，这样就可以避免进行用户态和内核态大量数据的拷贝，并且对于事件的发生不再通过轮询机制来获得，而是利用回调函数，在事件发生比如虚拟文件系统的读写接口被调用时，最终会通过不断的唤醒操作将等待该事件的进程唤醒，然后将得到的信息传给用户程序，这样的一个过程相对之前的轮询机制要快得多，因为事件发生之后对应的进程就能够获取对应的事件。

## libevent中使用epoll机制进行读写事件管理
采用epoll进行读写事件管理时比较方便，因为不再需要额外的数据结构来保存对应的事件，首先在初始化的时候需要创建一个epoll的管理器，这个管理器对应着一个文件描述符，调用的是`epoll_create()`接口。首先为什么这里会是一个文件描述符呢，因为epoll在内核中的实现是一个虚拟文件系统，也就是说`epoll_create`创建的也是一个文件，这个文件对应着一个epoll的管理器。
```c++
epoll_base::epoll_base()
	: event_base()
{
	if ((_epfd = epoll_create(1)) == -1)

	_epevents = new struct epoll_event[_nfds];
}

typedef union epoll_data {
    void        *ptr;
    int          fd;
    uint32_t     u32;
    uint64_t     u64;
} epoll_data_t;

struct epoll_event {
    uint32_t     events;      /* Epoll events */
    epoll_data_t data;        /* User data variable */
};
```
事件的添加比较容易，主要是通过`epoll_ctl`接口，并设置相应的参数就能够进行添加和删除，在事件分发时，epoll接口调用的是`epoll_wait`函数来等待事件通知，有事件时会将一个`struct epoll_event`的数组传回，然后根据返回值就可以进行相应的事件处理，总的来说`epoll`的使用非常方便，但是其实现原理却不这么容易。

## 内核epoll初始化
首先需要注意的是eventpoll是一个文件系统，作为内核的一个模块需要有着相应的初始化以及文件系统的注册操作，这些都是由`eventpoll_init`进行处理的，这里值得注意的是，在初始化的时候eventpoll虚拟文件系统在内核中创建了两块高速缓存，分别用于分配`struct epitem` 和 `struct eppoll_entry`结构，这两个结构是内核中进行epoll相关事件管理非常中要的结构，基本上`struct epitem`与要处理的fd相对应，在内核中使用高速缓存进行分配，可以极大的提升执行的效率。
```c++
static int __init eventpoll_init(void)
{
    epi_cache = kmem_cache_create("eventpoll_epi", sizeof(struct epitem), 0,
				      SLAB_HWCACHE_ALIGN | EPI_SLAB_DEBUG, NULL, NULL);
	pwq_cache = kmem_cache_create("eventpoll_pwq", sizeof(struct eppoll_entry), 0,
				      EPI_SLAB_DEBUG, NULL, NULL);
    error = register_filesystem(&eventpoll_fs_type);
	eventpoll_mnt = kern_mount(&eventpoll_fs_type);
}

module_init(eventpoll_init);
```

### epoll管理器的创建初始化-epoll_create
首先用户程序调用`epoll_create`会在内核中创建一个`struct eventpoll *ep`实例，用来管理相关的epoll事件，因为eventpoll是内核中的一个虚拟文件系统，所以实际上是创建了一个文件，并分配给系统中没有使用文件描述符进行关联。这里值得注意的是在文件的一个成员指针`private_data`用来保存ep，这样就能够将ep和eventpoll文件真正的关联在一起，以后通过文件描述符就可以获取到这个结构。
```c++
/*
 * It opens an eventpoll file descriptor by suggesting a storage of "size"
 * file descriptors. The size parameter is just an hint about how to size
 * data structures. It won't prevent the user to store more than "size"
 * file descriptors inside the epoll interface. It is the kernel part of
 * the userspace epoll_create(2).
 */
asmlinkage long sys_epoll_create(int size)
{
	struct inode *inode;
	struct file *file;
	/*
	 * Creates all the items needed to setup an eventpoll file. That is,
	 * a file structure, and inode and a free file descriptor.
	 */
	error = ep_getfd(&fd, &inode, &file);

	/* Setup the file internal data structure ( "struct eventpoll" ) */
	error = ep_file_init(file, hashbits);
	return fd;
}

static int ep_file_init(struct file *file, unsigned int hashbits)
{
	struct eventpoll *ep;
    // new *ep
	error = ep_init(ep, hashbits);
	file->private_data = ep;
	return 0;
}

static int ep_init(struct eventpoll *ep, unsigned int hashbits)
{
	init_waitqueue_head(&ep->wq);
	init_waitqueue_head(&ep->poll_wait);
	INIT_LIST_HEAD(&ep->rdllist);
	return 0;
}
```
如上一系列的初始化主要就是创建了`struct eventpoll` 对象，并初始化了其各成员，如`wq`、`poll_wait`及`rdlist`等分别对应着不同的队列或链表，内部实现其实都是链表。其中比较重要的如`wq`和`poll_wait`都是等待队列，为什么会有两个等待队列呢，首先`wq`这个等待队列保存的是eventpoll管理的事件的等待队列入口，而后者`poll_wait`则是用于管理eventpoll本身，因为eventpoll本身是一个虚拟文件系统，并且还实现了相应的`f_op->poll()`函数，说明eventpoll也能够被poll或者epoll所管理，所以其自身也需要一个等待队列管理自己。
```c++
/*
 * This structure is stored inside the "private_data" member of the file
 * structure and rapresent the main data sructure for the eventpoll
 * interface.
 */
struct eventpoll {
	wait_queue_head_t wq;           /* Wait queue used by sys_epoll_wait() */
	wait_queue_head_t poll_wait;    /* Wait queue used by file->poll() */

	struct list_head rdllist;       /* List of ready file descriptors */
	
	unsigned int hashbits;          /* Size of the hash */
	char *hpages[EP_MAX_HPAGES];    /* Pages for the "struct epitem" hash */
};
```

## 内核epoll中读写事件的加入-epoll_ctl
读写事件的添加和删除都是通过`epoll_ctl`来实现的，根据不同的参数提供不同的操作，对于添加和删除实际上是分别调用`ep_insert`等内部函数来实现的，当然这里也会有将用户态数据`struct epoll_event *`拷贝到内核的操作，epoll中这个拷贝相对poll和select内容要少的多。从如下可以看到，先根据传入的eventpoll文件的描述符`epfd`来获取管理结构ep，然后根据ep来将对应事件加入。

```c++
/*
 * The following function implements the controller interface for
 * the eventpoll file that enables the insertion/removal/change of
 * file descriptors inside the interest set.  It represents
 * the kernel part of the user space epoll_ctl(2).
 */
asmlinkage long sys_epoll_ctl(int epfd, int op, int fd, struct epoll_event __user *event)
{
	struct file *file, *tfile;
	struct eventpoll *ep;
	struct epoll_event epds;
	if (copy_from_user(&epds, event, sizeof(struct epoll_event)))

	file = fget(epfd);
	tfile = fget(fd);
	ep = file->private_data;

	switch (op) {
        case EPOLL_CTL_ADD:
                epds.events |= POLLERR | POLLHUP;
                error = ep_insert(ep, &epds, tfile, fd);
        case EPOLL_CTL_DEL:
        case EPOLL_CTL_MOD:
	}
}
```
传入内核的事件究竟以和中结构在内核中保存呢，上面提到传入的读写事件event通过复制内核并传给了`ep_insert`函数作为event参数。在内核epoll的实现中，实际上是以一个`struct epitem`结构来保存读写事件信息，这个结构如下所示，其中有着很多的链表入口，表示着这个结构可以作为多种不同链表的节点，这些链表分别包括由eventpoll管理的链表，事件准备好的链表，由文件管理的链表，以及最终传给用户空间的队列。
```c++
/*
 * Each file descriptor added to the eventpoll interface will
 * have an entry of this type linked to the hash.
 */
struct epitem {
	struct list_head llink;     // 由eventpoll管理
	struct list_head rdllink;   // 准备好了的事件链表
	struct list_head fllink;    // 由epitem对应的文件管理
	struct list_head txlink;    // 用于transfer队列，即最终传给用户空间的队列

	struct list_head pwqlist;   // 用于管理与epitem有关的Linux进程等待队列的管理器 eppoll_entry

	int nwait;              /* Number of active wait queue attached to poll operations */
	
	struct eventpoll *ep;   /* The "container" of this item */
	int fd;                 /* The file descriptor this item refers to */
	struct file *file;      /* The file this item refers to */
	struct epoll_event event;
};
```
如上结构中最难以理解的就是有一个`pwqlist`，这个队列中管理的内容实际上是与epitem有关的进程等待队列，这个队列中每个节点的内容如下可示。其中`llink`是由上面的epitem管理的链表，base指向epitem，而后面的wait和whead是真正管理等待进程队列的。
```c++
/* Wait structure used by the poll hooks */
struct eppoll_entry {
	struct list_head llink;     /* List header used to link this structure to the "struct epitem" */
	void *base;                 /* The "base" pointer is set to the container "struct epitem" */
	wait_queue_t wait;          // 进程等待队列
	wait_queue_head_t *whead;   // 将进程等待队列链接起来的链表入口
};
```

### epitem插入及等待进程队列初始化
在将epitem插入之前首先介绍一下poll机制的一些变化，我们知道，在以前版本的Linux的poll实现中，无论什么类型的文件，只要是实现了poll接口，允许poll机制的虚拟文件系统，其poll实现中必然会调用`poll_wait`函数，这个函数会将该进程加入到对应文件的等待队列中去。在Linux-2.6版本中，`poll_wait`函数依然存在，只不过新的实现照顾了epoll机制也要调用poll函数，这样epoll和poll可能有着不同的等待队列初始化的方式，所以在poll中就加入了如下的回调函数机制，允许poll和epoll分别注册自己的处理函数，然后在poll_wait中该处理函数会被调用，而poll_wait函数又会被所有的虚拟文件系统的poll函数所调用，所以这样epoll就能够重用poll机制了。
```c++
/* 
 * structures and helpers for f_op->poll implementations
 */
typedef void (*poll_queue_proc)(struct file *, wait_queue_head_t *, struct poll_table_struct *);

typedef struct poll_table_struct {
	poll_queue_proc qproc;
} poll_table;

static inline void poll_wait(struct file * filp, wait_queue_head_t * wait_address, poll_table *p)
{
	if (p && wait_address)
		p->qproc(filp, wait_address, p);
}

static inline void init_poll_funcptr(poll_table *pt, poll_queue_proc qproc)
{
	pt->qproc = qproc;
}
```
然后再看ep_insert函数就比较清楚的看到epitem的加入过程，首先从内核高速缓存分配一个epitem，然后初始化各个链表入口，并将epitem的成员设置成对应的参数，之后设置poll的回调函数`ep_ptable_queue_proc`，这样再调用poll之后就会回调该函数，之后将epi加入到两个链表，一个是tfile控制的链表，一个是eventpoll控制的哈希表中。再检查一下poll返回的值判断现在是否有事件已经发生，如果发生就将当前的epi加入到准备好了的链表rdllist中去，然后唤醒ep中保存的等待队列，这里值得注意的是这个`ep->wq`，也就是管理器eventpoll结构体中的等待队列wq，这个等待队列中其实是保存着与ep有关的等待进程队列，什么意思呢，如果只有一个单进程使用到这个eventpoll管理器的话，那么`ep->wq`中最多只可能有一个进程，也就是说，进程在使用ep管理全部的读写事件时，如果发现ep管理的文件描述符对应的读写事件都没发生，那么当前进程就会加入到`ep->wq`中，然后任何一个进程发现ep加入或删除事件有所变化就会唤醒`ep->wq`中所有等待的进程。
```c++
static int ep_insert(struct eventpoll *ep, struct epoll_event *event,struct file *tfile, int fd)
{
	int error, revents, pwake = 0;
	unsigned long flags;
	struct epitem *epi;
	struct ep_pqueue epq;

	// 从高速缓存分配epitem并初始化相关链表入口及成员
    // epi = EPI_MEM_ALLOC()
    // 各个link，及file fd event nwait等

	epq.epi = epi;
	init_poll_funcptr(&epq.pt, ep_ptable_queue_proc); // 注册epoll的poll机制回调函数 
	revents = tfile->f_op->poll(tfile, &epq.pt);      // poll最终会调用注册的回调函数

	// 将epi加入到文件tfile控制的链表中，对应fllink
	// 将epi加入到eventpoll控制的哈希表中

	/* 如果epi对应的传入的事件已经发生 */
	if ((revents & event->events) && !EP_IS_LINKED(&epi->rdllink)) {
		list_add_tail(&epi->rdllink, &ep->rdllist);

		/* Notify waiting tasks that events are available */
		if (waitqueue_active(&ep->wq))
			wake_up(&ep->wq);
		if (waitqueue_active(&ep->poll_wait))
			pwake++;
	}
}
```
#### 设置读写等待队列-ep_ptable_queue_proc
上面谈到与eventpoll管理器有关的等待队列，这个队列却与ep管理的文件是没有关系的，什么意思呢，当ep管理的文件发生读写时，上述`ep->wq`队列是不会被唤醒的，那么与文件读写有关的等待队列到底是哪个队列呢。实际的文件等待队列是通过如下的poll回调函数ep_ptable_queue_proc设置的。如下设置中最关键的又涉及到了之前提到的等待队列管理结构`struct eppoll_entry`，而pwq作为等待队列的管理结构也是从内核高速缓存分配的。
```c++
/*
 * This is the callback that is used to add our wait queue to the
 * target file wakeup lists.
 */
static void ep_ptable_queue_proc(struct file *file, wait_queue_head_t *whead, poll_table *pt)
{
	struct epitem *epi = EP_ITEM_FROM_EPQUEUE(pt);
	struct eppoll_entry *pwq;

	if (epi->nwait >= 0 && (pwq = PWQ_MEM_ALLOC())) {
		init_waitqueue_func_entry(&pwq->wait, ep_poll_callback);
		pwq->whead = whead;
		pwq->base = epi;
		add_wait_queue(whead, &pwq->wait);
		list_add_tail(&pwq->llink, &epi->pwqlist);
		epi->nwait++;
	} else {
		/* We have to signal that an error occurred */
		epi->nwait = -1;
	}
}
```
这里比较重要的一个问题就是等待队列`pwq->wait`应该加到哪里，还有就是`pwq->wait`中有些什么内容。先看看等待队列中有些什么内容，这里给等待队列初始化了一个回调函数ep_poll_callback，这个回调函数会在等待队列被唤醒时被调用，但是`pwq->wait`中关于进程的信息现在其实是没有的。另外一个问题`pwq->wait`被加到哪里去了，这里传入了一个`wait_queue_head_t *whead`参数，这个参数追本溯源其实是文件的等待队列，如下为以管道文件pipe为例的调用经过。
```c++
static unsigned int pipe_poll(struct file *filp, poll_table *wait)
{
	struct inode *inode = filp->f_dentry->d_inode;
	poll_wait(filp, PIPE_WAIT(*inode), wait);
    ...
}
```
在poll调用poll_wait之后，pipe文件的等待队列传到了回调函数ep_ptable_queue_proc中的参数`whead`，这下就通了，也就是epi的信息是在pipe文件的等待队列中的。

## 等待读写事件与进程的睡眠-epoll_wait
调用`epoll_wait`函数会调用内核`sys_poll_wait`函数，前面提到eventpoll管理器保存在`file->private_data`，所以这里根据管理器来调用`ep_poll`函数，在该函数中会判断是否有事件发生，如果没有就会将当前进程置于睡眠状态，放入等待队列中。
```c++
asmlinkage long sys_epoll_wait(int epfd, struct epoll_event __user *events,
			       int maxevents, int timeout)
{
	file = fget(epfd);
	ep = file->private_data;

	/* Time to fish for events ... */
	error = ep_poll(ep, events, maxevents, timeout);
}
```
可以看看ep_poll实际上做了什么，首先判断`ep->rdllist`是否为空，如果为空说明没有准备好的事件，之后就将当前进程current加入到等待队列`ep->wq`中，这个等待队列会通过之前注册的唤醒回调函数ep_poll_callback唤醒。后面进入循环的操作与poll和select类似，将当前进程状态设为TASK_INTERRUPTIBLE，表示可以被唤醒和中断。因为有一个等待队列`rdllist`，所以判断是否有事件发生也比较简单，只要该链表为空说明没有任何准备好的epitem。
```c++
static int ep_poll(struct eventpoll *ep, struct epoll_event __user *events,
		   int maxevents, long timeout)
{
	wait_queue_t wait;
    // 获取事件jtimeout
retry:
	if (list_empty(&ep->rdllist)) { // 没有任何准备好的事件
		/*
		 * We don't have any available event to return to the caller.
		 * We need to sleep here, and we will be wake up by
		 * ep_poll_callback() when events will become available.
		 */
		init_waitqueue_entry(&wait, current);
		add_wait_queue(&ep->wq, &wait);

		for (;;) {
			/*
			 * We don't want to sleep if the ep_poll_callback() sends us
			 * a wakeup in between. That's why we set the task state
			 * to TASK_INTERRUPTIBLE before doing the checks.
			 */
			set_current_state(TASK_INTERRUPTIBLE);
			if (!list_empty(&ep->rdllist) || !jtimeout) break;
			if (signal_pending(current)) { res = -EINTR; break; }
			jtimeout = schedule_timeout(jtimeout); // schedule调度，释放cpu
		}
		remove_wait_queue(&ep->wq, &wait);
		set_current_state(TASK_RUNNING);
	}
    ep_events_transfer(ep, events, maxevents))
	return res;
}
```

## 读写事件的发生与Linux等待进程的唤醒机制
先看看Linux-2.6中的等待机制，就等待队列来说，核心的数据结构就是`wait_queue_t`，其主要内容包括进程指针task以及等待回调函数，这样的好处是如epoll可以不通过task来唤醒自己，而是可以给等待队列注册一个回调函数，在这个函数中自定义要唤醒的进程队列。
```c++
struct __wait_queue {
	unsigned int flags;
	struct task_struct * task;
	wait_queue_func_t func;
	struct list_head task_list;
};
```
首先对于只是注册了进程指针task的等待队列，会有一个默认的唤醒回调函数default_wake_function，这个默认唤醒函数会调用`try_to_wake_up`来真正的唤醒进程。
```c++
int default_wake_function(wait_queue_t *curr, unsigned mode, int sync)
{
	task_t *p = curr->task;
	return try_to_wake_up(p, mode, sync);
}

static void __wake_up_common(wait_queue_head_t *q, unsigned int mode, int nr_exclusive, int sync)
{
	struct list_head *tmp, *next;

	list_for_each_safe(tmp, next, &q->task_list) {
		wait_queue_t *curr;
		unsigned flags;
		curr = list_entry(tmp, wait_queue_t, task_list);
		flags = curr->flags;
		if (curr->func(curr, mode, sync) &&
		    (flags & WQ_FLAG_EXCLUSIVE) &&
		    !--nr_exclusive)
			break;
	}
}
```

### 两条等待队列先后唤醒
前面提到的是在eventpoll实现中会有两条主要的等待队列，一条是与文件相关的等待队列，这个等待队列中保存了所有因该文件而睡眠的进程队列注册的`wait_queue_t`，如果该文件发生如读写事件的话，会`wake_up`这条队列。以pipe的写入为例，在pipe的write实现`pipe_write`中，如果确认发生了写入数据，则会调用`wake_up_interruptible`或类似函数唤醒该文件的等待队列`PIPE_WAIT(*inode)`，而这条队列在之前的ep_ptable_queue_proc函数中正好插入了epi对应的等待队列`pwq->wait`，这个wait中只注册了唤醒回调函数`ep_poll_callback`，该唤醒函数会执行。从上面的`__wake_up_common`函数可以看到，遍历文件的等待队列会执行每个节点注册的回调函数，默认情况下调用默认唤醒函数执行真正的唤醒进程的操作`try_to_wake_up`，但是我们eventpoll机制中epi注册的等待队列，只是注册了自己的唤醒回调函数，所以会调用自己的唤醒函数。

这就涉及到第二条队列的唤醒，可以看到之前注册的唤醒函数的实现`ep_poll_callback`，首先根据传入的wait获取到epi，然后根据epi又可以获取到ep，然后将epi加入到ep的`rdllist`中，表示该epi对应的事件已经准备好了，加入之后就可以唤醒`ep->wq`，这就是第二条队列，我们知道前面在`ep_poll`中进入循环前将进程加入到`ep->wq`中了，这时候才真正的唤醒ep中管理的等待进程。
```c++
/*
 * This is the callback that is passed to the wait queue wakeup
 * machanism. It is called by the stored file descriptors when they
 * have events to report.
 */
static int ep_poll_callback(wait_queue_t *wait, unsigned mode, int sync)
{
	struct epitem *epi = EP_ITEM_FROM_WAIT(wait);
	struct eventpoll *ep = epi->ep;

	/* If this file is already in the ready list we exit soon */
	if (EP_IS_LINKED(&epi->rdllink))
		goto is_linked;

	list_add_tail(&epi->rdllink, &ep->rdllist);

is_linked:
	/*
	 * Wake up ( if active ) both the eventpoll wait list and the ->poll()
	 * wait list.
	 */
	if (waitqueue_active(&ep->wq))
		wake_up(&ep->wq);
	if (waitqueue_active(&ep->poll_wait))
		pwake++;

	/* We have to call this outside the lock */
	if (pwake)
		ep_poll_safewake(&psw, &ep->poll_wait);

	return 1;
}
```

## 获取到发生的事件并传送给用户程序
上面知道现在等待进程已经被唤醒，唤醒之后继续在`ep_poll`函数中执行下一次循环，这次判断`ep->rdllist`是不为空的，因为在上面唤醒函数ep_poll_callback中已经将准备好的epi加入到了准备好的队列中了，所以这次有数据退出循环，先把当前进程从等待队列中移除，重新设置进程状态为可调度，再之后执行`ep_events_transfer`才真正的将获取到的事件传送给用户程序，这个events是用户程序的`struct event_poll `指针。
```c++
// ep_poll
		for (;;) {
			set_current_state(TASK_INTERRUPTIBLE);
			if (!list_empty(&ep->rdllist) || !jtimeout) break;
			if (signal_pending(current)) { res = -EINTR; break; }
			jtimeout = schedule_timeout(jtimeout); // schedule调度，释放cpu
		}
		remove_wait_queue(&ep->wq, &wait);
		set_current_state(TASK_RUNNING);
	}
    ep_events_transfer(ep, events, maxevents))
	return res;
}

/* Perform the transfer of events to user space. */
static int ep_events_transfer(struct eventpoll *ep,
			      struct epoll_event __user *events, int maxevents)
{
	int eventcnt = 0;
	struct list_head txlist;

	INIT_LIST_HEAD(&txlist);

	/* Collect/extract ready items 收集item的时候将epi加入txlist，然后会从rdllist移除 */
	if (ep_collect_ready_items(ep, &txlist, maxevents) > 0) {
		/* Build result set in userspace */
		eventcnt = ep_send_events(ep, &txlist, events);

		/* Reinject ready items into the ready list */
		ep_reinject_items(ep, &txlist);
	}

	return eventcnt;
}

```
传送事件简单来说就是首先收集ep中的准备好的链表，然后用txlist链表来汇总，然后遍历txlist来将数据都复制到用户空间指针`struct event_poll *event`。

### 水平触发和边缘触发
关于水平触发和边缘触发模式，其实与如何通知用户程序事件发生有关，对于select和poll机制只有水平触发模式，为什么呢，因为select和poll机制会不断的轮询文件描述符，调用底层的poll函数时如果发现有数据可读或可写，就会退出`do_select`或`do_poll`函数，这样也就是说只要轮询到描述符可以读或者可以写就会通知用户程序。这样的方式在描述符比较少的时候比较好，而且在少量的描述符很活跃的时候能够持续的进行监控该描述符，对于像tcp通信中的listen和accept这样的操作，使用水平触发比较好。

而对于需要监控大量描述符进行大规模并发的程序来说，使用边沿触发就能够取得非常好的性能。比如对于epoll机制来说，既存在水平触发模式也存在边缘触发，默认情况下采用水平触发，虽然epoll机制内部使用回调机制使得对大规模io复用已经比poll机制要好，但是如果使用水平触发模式，每次文件描述符有数据就通知用户程序，这样如果每次用户操作都只读取少量的数据，文件描述符中一直都有多余的数据，那么就需要不断的通知用户程序，这样其实是很低效的。所以epoll提供了边缘触发模式，也就是只在该文件描述符发生如读写事件时才通知用户程序有事件发生了。

在内核中epoll的两种模式的实现其实比较容易，主要区别就是判断在将事件传给用户程序之后是否继续让该epitem留在准备好的队列中，上面`ep_events_transfer`中，使用`ep_send_events`传给用户程序事件之后有一个重新插入的操作，也就是ep_reinject_items函数。这个函数在将epi从传送队列中一个个解下来的同时会判断是否要重新插入rdllist，关键就在于判断的条件，首先判断`epi->llink`，即判断该epi是否还在eventpoll管理器中，然后判断没有设置`EPOLLET`，EPOLLET也就是边缘触发标志(Edge Triggering)，然后判断返回的事件中包括注册的事件，最后判断这个epi还不在准备队列中，全部满足的话就将其重新加入rdllist中，这一过程说明如果没有在用户程序中主动设置EPOLLET标志的话，使用的是水平触发，epi会重新加入到rdllist中，然后再次执行ep_poll的时候就不会进入for循环了，因为rdllist不空，需要将其中的事件传给用户程序。那么问题来了，水平触发的话什么时候rdllist中才会删除该epi呢，其实还是看这个判断条件，删除的话在前面的收集操作ep_collect_ready_items中会从rdllist中删除epi，现在只要poll操作返回的事件revents中不包含epi注册的事件`epi->event.events`，epi就不会再次加入rdllist中，也就是说文件描述符不再可读或可写rdllist最终会为空。这里的一个问题就是，只要用户程序读取文件描述符的速度追不上其它程序往里面写入的数据的话，epoll机制就会一直不停的通知用户程序文件描述符可读。如果设置成边缘触发的话就会在通知一次之后，等到下次其他程序写入的时候epoll才会通知当前进程可以读了，避免了不停地通知。

```c++
/*
 * Walk through the transfer list we collected with ep_collect_ready_items()
 * and, if 1) the item is still "alive" 2) its event set is not empty 3) it's
 * not already linked, links it to the ready list. Same as above, we are holding
 * "sem" so items cannot vanish underneath our nose.
 */
static void ep_reinject_items(struct eventpoll *ep, struct list_head *txlist)
{
	while (!list_empty(txlist)) { // 遍历传输链表
		epi = list_entry(txlist->next, struct epitem, txlink);
		EP_LIST_DEL(&epi->txlink); // 从传输链表中删除

		/*
		 * If the item is no more linked to the interest set, we don't
		 * have to push it inside the ready list because the following
		 * ep_release_epitem() is going to drop it. Also, if the current
		 * item is set to have an Edge Triggered behaviour, we don't have
		 * to push it back either.
		 */
		if (EP_IS_LINKED(&epi->llink) && !(epi->event.events & EPOLLET) &&
		    (epi->revents & epi->event.events) && !EP_IS_LINKED(&epi->rdllink)) {
			list_add_tail(&epi->rdllink, &ep->rdllist);
			ricnt++;
		}
	}
	if (ricnt)  
		// 存在再次加入准备好了的队列的epitem，所以rdllist有数据，唤醒 ep->wq 
}
```

## 小结
总的来说，epoll事件的一整个生命周期到此就结束了，整个机制中比较重要的有两个方面，首先是两个回调函数，一个用于poll机制的回调函数，`poll_wait`中会调用poll或epoll注册的回调函数，而`poll_wait`又会在任何实现了poll的虚拟文件系统中调用，epoll机制注册的回调函数会将epi加入到文件的等待队列中。另外一个回调函数也就是唤醒机制的回调函数，这个epoll注册了自己的回调函数，使得文件在执行write的时候调用内核唤醒函数，最终是执行了epoll的回调函数，而epoll的回调函数会执行自己的唤醒过程。这时候其实就涉及到两条队列，在eventpoll结构中的`ep->wq`实际上保存了等待进程的指针，而前面提到的epi中等待队列的唤醒会调用注册的唤醒函数，这个唤醒函数会唤醒`ep->wq`队列，最终调用默认唤醒函数唤醒等待的进程。

那么，epoll机制相对poll或者select机制优点到底在哪里呢，我们知道poll和select机制在循环中会不断的轮询要关注的文件描述符和对应的事件，而epoll机制是不需要的。首先epoll机制在循环中是根据准备好队列`ep->rdllist`是否为空来判断是否有事件发生的，这样避免了轮询。其次在epoll对于要关注的文件描述符和事件由内核高速缓存进行管理，避免了用户空间和内核空间的数据拷贝，提升效率。另外最关键的部分在于，epoll机制中在文件读写时调用唤醒函数，如果存在epoll的等待队列，那么会执行回调唤醒函数，这个epoll的唤醒函数会将发生了事件的epi直接加入到`ep->rdllist`，这样准备好了的队列就可以立即由空变为有事件发生。基于上述种种优点，epoll机制相对以前的poll和select能够进行大规模的并发。