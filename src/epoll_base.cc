#include "epoll_base.hh"
#include "rw_event.hh"

#include <sys/resource.h>

namespace eve
{

epoll_base::epoll_base()
	: event_base()
{
	std::cout << __PRETTY_FUNCTION__ << std::endl;
	struct rlimit rl;
	if (getrlimit(RLIMIT_NOFILE, &rl) == 0 && rl.rlim_cur != RLIM_INFINITY)
		_nfds = rl.rlim_cur;

	if ((_epfd = epoll_create(1)) == -1)
		std::cerr << "epoll_create\n";

	_epevents = new struct epoll_event[_nfds];
}

int epoll_base::recalc(int max)
{
	std::cout << __PRETTY_FUNCTION__ << std::endl;
	return evsignal_recalc();
}

int epoll_base::dispatch(struct timeval *tv)
{
	std::cout << __PRETTY_FUNCTION__ << std::endl;
	if (evsignal_deliver() == -1)
		return -1;

	int timeout = -1;
	if (tv)
		timeout = tv->tv_sec * 1000 + (tv->tv_usec + 999) / 1000;
	int res = epoll_wait(_epfd, _epevents, _nfds, timeout);

	if (evsignal_recalc() == -1)
		return -1;

	if (res == -1)
	{
		if (errno != EINTR)
		{
			std::cerr << "epoll_wait\n";
			return -1;
		}
		evsignal_process();
		return 0;
	}
	else if (caught)
		evsignal_process();

	int what = 0;
	rw_event *ev;
	for (int i = 0; i < res; i++)
	{
		what = _epevents[i].events;
		ev = (rw_event *)_epevents[i].data.ptr;
		if (what & (EPOLLHUP | EPOLLERR))
			what |= (EPOLLIN | EPOLLOUT);

		if (what && ev)
		{
			if ((what & EPOLLIN) && ev->is_readable())
				ev->set_active_read();
			if ((what & EPOLLOUT) && ev->is_writable())
				ev->set_active_write();

			if (ev->has_active_read() || ev->has_active_write())
			{
				if (!ev->is_persistent())
					ev->del();
				ev->activate(1);
			}
		}
	}
	return 0;
}

int epoll_base::add(rw_event *ev)
{
	std::cout << __PRETTY_FUNCTION__ << std::endl;
	fd_map_rw[ev->fd] = ev;

	struct epoll_event epev = {0, {0}};
	epev.data.ptr = ev;
	if (ev->is_readable())
		epev.events |= EPOLLIN;
	if (ev->is_writable())
		epev.events |= EPOLLOUT;
	if (epoll_ctl(_epfd, EPOLL_CTL_ADD, ev->fd, &epev) == -1)
		return -1;
}

int epoll_base::del(rw_event *ev)
{
	std::cout << __PRETTY_FUNCTION__ << std::endl;
	fd_map_rw.erase(ev->fd);

	struct epoll_event epev = {0, {0}};
	if (ev->is_readable())
		epev.events |= EPOLLIN;
	if (ev->is_writable())
		epev.events |= EPOLLOUT;
	if (epoll_ctl(_epfd, EPOLL_CTL_DEL, ev->fd, &epev) == -1)
		return -1;

	return 0;
}

} // namespace eve
