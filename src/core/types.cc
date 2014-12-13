/******************************************************************************
  Synopsis		[Bfc - Greedy Analysis of Multi-Threaded Programs with 
				Non-Blocking Communication.]

  Author		[Alexander Kaiser]

(C) 2011 - 2014 Alexander Kaiser, University of Oxford, United Kingdom

All rights reserved. Redistribution and use in source and binary forms, with
or without modification, are permitted provided that the following
conditions are met:

  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.

  3. All advertising materials mentioning features or use of this software
     must display the following acknowledgement:

     This product includes software developed by Alexander Kaiser, 
     University of Oxford, United Kingdom and its contributors.

  4. Neither the name of the University nor the names of its contributors
     may be used to endorse or promote products derived from this software
     without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS `AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
******************************************************************************/
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

//http://stackoverflow.com/questions/6089231/getting-std-ifstream-to-handle-lf-cr-and-crlf
std::istream& safeGetline(std::istream& is, std::string& t)
{
    t.clear();

    // The characters in the stream are read one-by-one using a std::streambuf.
    // That is faster than reading them one-by-one using the std::istream.
    // Code that uses streambuf this way must be guarded by a sentry object.
    // The sentry object performs various tasks,
    // such as thread synchronization and updating the stream state.

    std::istream::sentry se(is, true);
    std::streambuf* sb = is.rdbuf();

    for(;;) {
        int c = sb->sbumpc();
        switch (c) {
        case '\n':
            return is;
        case '\r':
            if(sb->sgetc() == '\n')
                sb->sbumpc();
            return is;
        case EOF:
            // Also handle the case when the last line has no line ending
            if(t.empty())
                is.setstate(std::ios::eofbit);
            return is;
        default:
            t += (char)c;
        }
    }
}
