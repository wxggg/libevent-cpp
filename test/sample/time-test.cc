#include <time_event.hh>
#include <select_base.hh>
#include <poll_base.hh>
#include <iostream>

using namespace std;
using namespace eve;

void timeout_cb(event *argev)
{
	time_event *timeout = (time_event *)argev;
	int newtime = time(NULL);

	cout << __func__ << ": called at " << newtime << endl;

	timeout->set_timer(3, 0);
	timeout->add();
}


int main(int argc, char **argv)
{
	select_base base;
	// poll_base base;

	time_event timeout(&base);
	timeout.set_callback(timeout_cb);
	timeout.set_timer(5, 0);
	timeout.add();

	base.loop();
}
