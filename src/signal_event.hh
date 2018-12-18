#include "event.hh"

namespace eve
{
class signal_event : public event
{
public:
	int sig = -1;

public:
	signal_event(event_base *base);
	~signal_event() {}

	inline void set_sig(int sig) { this->sig = sig; }

	inline void set(int sig, void (*callback)(event *))
	{
		this->sig = sig;
		this->callback = callback;
	}

	void add();
	void del();
};

} // namespace eve
