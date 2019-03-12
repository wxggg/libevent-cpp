#include <time_event.hh>
#include <select_base.hh>
#include <poll_base.hh>
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


int main(int argc, char **argv)
{
	// select_base base;
	auto base = std::make_shared<select_base>();

	auto ev = create_event<time_event>(base);
	base->register_callback(ev, timeout_cb, ev);
	ev->set_timer(5, 0);
	base->add_event(ev);

	base->loop();
}
