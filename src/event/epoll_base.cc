#include <epoll_base.hh>
#include <rw_event.hh>

#include <sys/resource.h>

namespace eve
{

epoll_base::epoll_base()
{
	struct rlimit rl;
	if (getrlimit(RLIMIT_NOFILE, &rl) == 0 && rl.rlim_cur != RLIM_INFINITY)
		_nfds = rl.rlim_cur;

	if ((_epfd = epoll_create(1)) == -1)
		LOG_ERROR << "epoll_create\n";

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
			LOG_ERROR << "epoll_wait error";
			return -1;
		}
		evsignal_process();
		return 0;
	}
	else if (caught)
		evsignal_process();

	int what = 0;
	for (int i = 0; i < res; i++)
	{
		what = _epevents[i].events;
		auto ev = fdMapRw.at(_epevents[i].data.fd);
		if (what & (EPOLLHUP | EPOLLERR))
			what |= (EPOLLIN | EPOLLOUT);

		if (what && ev)
		{
			ev->clear_active();
			if ((what & EPOLLIN) && ev->is_readable())
				ev->set_active_read();
			if ((what & EPOLLOUT) && ev->is_writeable())
				ev->set_active_write();

			if (ev->is_read_active() || ev->is_write_active())
			{
				if (!ev->is_persistent())
					remove_event(ev);
				activate(ev, 1);
			}
		}
	}
	return 0;
}

int epoll_base::add(std::shared_ptr<rw_event> ev)
{
	struct epoll_event epev = {0, {0}};
	epev.data.fd = ev->fd;
	int op = EPOLL_CTL_ADD;

	if (ev->epoll_in || ev->epoll_out)
		op = EPOLL_CTL_MOD;

	if (ev->epoll_in || ev->is_readable())
		epev.events |= EPOLLIN;
	if (ev->epoll_out || ev->is_writeable())
		epev.events |= EPOLLOUT;

	if (epoll_ctl(_epfd, op, ev->fd, &epev) == -1)
	{
		LOG_ERROR << "epoll_ctl error";
		return -1;
	}

	ev->epoll_in |= ev->is_readable();
	ev->epoll_out |= ev->is_writeable();
	return 0;
}

int epoll_base::del(std::shared_ptr<rw_event> ev)
{
	struct epoll_event epev = {0, {0}};
	epev.data.fd = ev->fd;

	bool writedelete = true, readdelete = true;

	int op = EPOLL_CTL_DEL;

	int events = 0;

	if (!ev->is_readable())
		events |= EPOLLIN;
	if (!ev->is_writeable())
		events |= EPOLLOUT;

	if ((events & (EPOLLIN | EPOLLOUT)) != (EPOLLIN | EPOLLOUT))
	{
		if ((events & EPOLLIN) && ev->epoll_out)
		{
			writedelete = false;
			events = EPOLLOUT;
			op = EPOLL_CTL_MOD;
		}
		else if ((events & EPOLLOUT) && ev->epoll_in)
		{
			readdelete = false;
			events = EPOLLIN;
			op = EPOLL_CTL_MOD;
		}
	}

	epev.events = events;

	if (writedelete)
		ev->epoll_out = false;
	if (readdelete)
		ev->epoll_in = false;

	if (epoll_ctl(_epfd, op, ev->fd, &epev) == -1)
	{
		std::cout << "fd=" << ev->fd << std::endl;
		LOG_ERROR << "epoll_ctl error with errno=" << errno;
		perror("??");
		exit(-1);
		return -1;
	}

	return 0;
}

} // namespace eve
