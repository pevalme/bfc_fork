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

interrupt_t exe_state = RUNNING;

bool ostream_sync::force_flush = false;
boost::mutex ostream_sync::shared_cout_mutex;

streambuf * ostream_sync::rdbuf(streambuf * a){
	return os.rdbuf(a);
}

ostream_sync::ostream_sync(streambuf * a) : os(a) {
}

ostream_sync::ostream_sync(std::ostream& os) : os(os.rdbuf()) {
}

ostream_sync::~ostream_sync() {
	if(os.rdbuf())
	{
		invariant(os.rdbuf() && ss.rdbuf() && ss.good());
		os << ss.rdbuf() << std::flush; // write the whole shebang in one go
	}
}

ostream_sync& operator<<(ostream_sync& os, ostream_manipulator pf)
{
	if(os.os.rdbuf()) 
		operator<< <ostream_manipulator> (os, pf);
	
	return os;
}

void ostream_sync::weak_flush(){
#if 0
	flush();
#endif
}

void ostream_sync::flush(){
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

void ostream_sync::set_force_flush(bool b)
{
	force_flush = b;
}

void ostream_sync::swap(ostream_sync& other)
{
	os.rdbuf(other.os.rdbuf());
#ifdef WIN32
	ss.swap(other.ss);
#endif
}

insert_t::insert_t(po_rel_t c, bstate_t i, s_scpc_t il): case_type(c), new_el(i), ex(il)
{
}

insert_t::insert_t(po_rel_t c, bstate_t i, bstate_t il): case_type(c), new_el(i), ex()
{
	ex.insert(il); 
}

insert_t::insert_t()
{
}

string add_leading_zeros(const string& s, unsigned count)
{
	int diff = count-s.size();
	if(diff > 0) return string(diff,'0') + s;
	else return s;
}
