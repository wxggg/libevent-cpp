#include <time_event.hh>
#include <select_base.hh>
#include <poll_base.hh>
#include <iostream>

using namespace std;
using namespace eve;

void timeout_cb(time_event *ev)
{
	int newtime = time(nullptr);

	cout << __func__ << ": called at " << newtime << endl;

	ev->set_timer(3, 0);
	ev->add();
}


int main(int argc, char **argv)
{
	// select_base base;
	auto base = std::make_shared<select_base>();
	// poll_base base;

	time_event timeout(base);
	timeout.set_callback(timeout_cb, &timeout);
	timeout.set_timer(5, 0);
	timeout.add();

	base->loop();
}
