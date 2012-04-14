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


void FullExpressionAccumulator::rdbuf(basic_streambuf<char, std::char_traits<char> >* a){
	os.rdbuf(a);
}

FullExpressionAccumulator::FullExpressionAccumulator(basic_streambuf<char, std::char_traits<char> >* a) : os(a) {
	//ss << "----------------------------" << "\n";
}

FullExpressionAccumulator::FullExpressionAccumulator(std::ostream& os) : os(os.rdbuf()) {
	//ss << "----------------------------" << "\n";
}

FullExpressionAccumulator::~FullExpressionAccumulator() {
	if(os.rdbuf()) 
		os << ss.rdbuf() << std::flush; // write the whole shebang in one go
}

void FullExpressionAccumulator::flush()
{
	int size;
	if(os.rdbuf()) {
		ss.seekg(0, ios::end), size = ss.tellg(), ss.seekg(0, ios::beg);
		if(size){

			//ss << "----------------------------" << "\n";

			shared_cout_mutex.lock();
			os << ss.rdbuf() << std::flush;
			ss.str( std::string() ), ss.clear(); //http://stackoverflow.com/questions/2848087/how-to-clear-stringstream

			//ss << "----------------------------" << "\n";

			shared_cout_mutex.unlock();
		}
	}
}

FullExpressionAccumulator maincout(cout.rdbuf());
FullExpressionAccumulator maincerr(cout.rdbuf());
