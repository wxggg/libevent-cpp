#include "event.hh"

namespace eve
{
class signal_event : public event
{
  public:
	int sig = -1;

  public:
	signal_event(std::shared_ptr<event_base> base) : event(base) {}
	signal_event(std::shared_ptr<event_base> base, int sig) : event(base), sig(sig) {}
	~signal_event();

	inline void set_sig(int sig) { this->sig = sig; }


	int add() override;
	int del() override;
};

} // namespace eve
