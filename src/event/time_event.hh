#pragma once

#include <event.hh>

namespace eve
{

class time_event : public event
{

  public:
	struct timeval timeout;

  public:
	time_event(std::shared_ptr<event_base> base);
	~time_event();

	void set_timer(int sec, int usec);

	int add() override;
	int del() override;
};

} // namespace eve
