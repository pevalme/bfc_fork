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

//std::ostream fwinfo(cout.rdbuf());
//std::ostream fwtrace(cout.rdbuf());
//std::ostream dout(cout.rdbuf());
//std::ostream bout(cout.rdbuf());
//std::ostream fwstatsout(cout.rdbuf());
//std::ostream bwstatsout(cout.rdbuf());
//std::ostream ttsstatsout(cout.rdbuf());

boost::mutex shared_cout_mutex;

graph_type_t graph_type, tree_type;

interrupt_t execution_state = RUNNING;
