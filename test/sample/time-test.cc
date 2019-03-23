#include <time_event.hh>
#include <select_base.hh>
#include <poll_base.hh>
#include <signal_event.hh>

#include <iostream>

using namespace std;
using namespace eve;

void timeout_cb(std::shared_ptr<time_event> ev)
{
	int newtime = time(nullptr);

	cout << __func__ << ": called at " << newtime << endl;

	ev->set_timer(3, 0);
	ev->get_base()->add_event(ev);
}

void signal_cb(std::shared_ptr<signal_event> ev)
{
	ev->get_base()->set_terminated();
}

int main(int argc, char **argv)
{
	// select_base base;
	auto base = std::make_shared<select_base>();

	auto ev = create_event<time_event>(base);
	base->register_callback(ev, timeout_cb, ev);
	ev->set_timer(5, 0);
	base->add_event(ev);

	auto sev = create_event<signal_event>(base, SIGINT);
	base->register_callback(sev, signal_cb, sev);
	base->add_event(sev);

	base->loop();
}
