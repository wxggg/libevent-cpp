#pragma once

#include <event.hh>

namespace eve
{

class time_event : public event
{

public:
	struct timeval timeout;

public:
	time_event(std::shared_ptr<event_base> base) : event(base) { timerclear(&timeout); }
	~time_event() {}

	void set_timer(int sec, int usec)
	{
		struct timeval now, tv;
		gettimeofday(&now, nullptr);

		timerclear(&tv);
		tv.tv_sec = sec;
		tv.tv_usec = usec;

		timeradd(&now, &tv, &timeout);
	}
};

} // namespace eve
