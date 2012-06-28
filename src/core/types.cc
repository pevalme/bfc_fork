#include "types.h"

#include "user_assert.h"

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

boost::mutex fwbw_mutex;

graph_type_t graph_type, tree_type;

interrupt_t execution_state = RUNNING;

bool FullExpressionAccumulator::force_flush = false;

streambuf * FullExpressionAccumulator::rdbuf(streambuf * a){
	return os.rdbuf(a);
}

FullExpressionAccumulator::FullExpressionAccumulator(streambuf * a) : os(a) {
}

FullExpressionAccumulator::FullExpressionAccumulator(std::ostream& os) : os(os.rdbuf()) {
}

FullExpressionAccumulator::~FullExpressionAccumulator() {
	if(os.rdbuf())
	{
		invariant(os.rdbuf() && ss.rdbuf() && ss.good());
		os << ss.rdbuf() << std::flush; // write the whole shebang in one go
	}
}

void FullExpressionAccumulator::weak_flush(){
#if 0
	flush();
#endif
}

void FullExpressionAccumulator::flush(){
	std::streamoff size;
	if(os.rdbuf()) {
		ss.seekg(0, ios::end), size = ss.tellg(), ss.seekg(0, ios::beg);
		if(size){
			shared_cout_mutex.lock();
			os << ss.rdbuf() << std::flush;
			ss.str( std::string() ), ss.clear(); //http://stackoverflow.com/questions/2848087/how-to-clear-stringstream
			shared_cout_mutex.unlock();
		}
	}
}

void FullExpressionAccumulator::set_force_flush(bool b)
{
	force_flush = b;
}


