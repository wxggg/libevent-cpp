#include <epoll_base.hh>
#include "rw_event.hh"

#include <sys/resource.h>

namespace eve
{

epoll_base::epoll_base()
{
	struct rlimit rl;
	if (getrlimit(RLIMIT_NOFILE, &rl) == 0 && rl.rlim_cur != RLIM_INFINITY)
		_nfds = rl.rlim_cur;

	if ((_epfd = epoll_create(1)) == -1)
		std::cerr << "epoll_create\n";

	_epevents = new struct epoll_event[_nfds];
}

epoll_base::~epoll_base()
{
	delete[] _epevents;
}

int epoll_base::recalc()
{
	return evsignal_recalc();
}

int epoll_base::dispatch(struct timeval *tv)
{
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
			ev->clear_active();
			if ((what & EPOLLIN) && ev->is_readable())
				ev->activate_read();
			if ((what & EPOLLOUT) && ev->is_writable())
				ev->activate_write();

			if (ev->is_read_active() || ev->is_write_active())
				ev->activate(1);
		}
	}
	return 0;
}

int epoll_base::add(rw_event *ev)
{
	struct epoll_event epev = {0, {0}};
	epev.data.ptr = ev;
	int op = EPOLL_CTL_ADD;

	if (ev->is_read_available() || ev->epoll_in)
		epev.events |= EPOLLIN;
	if (ev->is_write_available() || ev->epoll_out)
		epev.events |= EPOLLOUT;

	if (!ev->is_read_write())
		return epoll_ctl(_epfd, op, ev->fd, &epev);

	if (ev->epoll_in || ev->epoll_out)
		op = EPOLL_CTL_MOD;

	if (epev.events & EPOLLIN)
		ev->epoll_in = true;
	if (epev.events & EPOLLOUT)
		ev->epoll_out = true;

	if (epoll_ctl(_epfd, op, ev->fd, &epev) == -1)
		return -1;
	return 0;
}

int epoll_base::del(rw_event *ev)
{
	struct epoll_event epev = {0, {0}};
	epev.data.ptr = ev;
	int events = 0;
	int op = EPOLL_CTL_DEL;

	if (!ev->is_read_available())
		events |= EPOLLIN;
	if (!ev->is_write_available())
		events |= EPOLLOUT;

	if (!ev->is_read_write())
		return epoll_ctl(_epfd, op, ev->fd, &epev);

	if ((events & (EPOLLIN | EPOLLOUT)) != (EPOLLIN | EPOLLOUT))
	{
		if ((events & EPOLLIN) && ev->epoll_out)
		{
			events = EPOLLOUT;
			op = EPOLL_CTL_MOD;
			ev->epoll_out = false;
		}
		else if ((events & EPOLLOUT) && ev->epoll_in)
		{
			events = EPOLLIN;
			op = EPOLL_CTL_MOD;
			ev->epoll_in = false;
		}
	}

	if (op == EPOLL_CTL_DEL)
	{
		if (events & EPOLLIN)
			ev->epoll_in = false;
		if (events & EPOLLOUT)
			ev->epoll_out = false;
	}

	epev.events = events;

	if (epoll_ctl(_epfd, op, ev->fd, &epev) == -1)
		return -1;

	return 0;
}

} // namespace eve
