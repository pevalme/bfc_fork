#include "types.h"

using namespace std;

string info(string s)
{
	return string("INFO: ") + s;
}

string fatal(string s)
{
	return string("FATAL ERROR: ") + s;
}

string warning(string s)
{
	return string("WARNING: ") + s;
}

ostream inf(cout.rdbuf());
ostream fwinfo(cout.rdbuf());
ostream preinf(cout.rdbuf());
ostream bout(cout.rdbuf());
ostream dout(cout.rdbuf());
ostream statsout(cout.rdbuf());

boost::mutex shared_cout_mutex;

graph_type_t graph_type;

interrupt_t execution_state = RUNNING;
