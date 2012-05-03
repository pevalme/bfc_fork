#ifndef TYPES_H
#define TYPES_H

#ifndef WIN32
#define nullptr 0
#endif

#include "multiset_adapt.h"

#include <boost/foreach.hpp>
#define foreach(a,b) BOOST_FOREACH(a,b)
#define foreachit(walker,cont) for(auto walker=cont.begin(),e=cont.end();walker!=e;++walker)

typedef const          short  cshort;
typedef const          int    cint;
typedef const          long   clong;
typedef       unsigned short  ushort;
typedef       unsigned int    uint;
typedef       unsigned long   ulong;
typedef const unsigned short cushort;
typedef const unsigned int   cuint;
typedef const unsigned long  culong;

//typedef unsigned short	shared_t;
//typedef unsigned short	local_t;
typedef unsigned shared_t;
typedef unsigned local_t;

const shared_t invalid_shared = (shared_t)-1;
const local_t invalid_local = (local_t)-2;

#include <set>
typedef std::multiset<local_t> bounded_t;
typedef std::set<local_t> unbounded_t;

#ifdef ICPC
#include <boost/unordered_set.hpp>
using boost::unordered_set;
#include <boost/unordered_map.hpp>
using boost::unordered_map;
#else
#include <unordered_set>
using std::unordered_set;
#include <unordered_map>
using std::unordered_map;
#endif

#include <string>
std::string info(std::string);
std::string fatal(std::string);
std::string warning(std::string);

//shared data
#include <iostream>

#include <boost/thread.hpp>
extern boost::mutex fwbw_mutex;

enum graph_type_t {GTYPE_TIKZ,GTYPE_DOT,GTYPE_NONE};
extern graph_type_t graph_type, tree_type;

enum interrupt_t { RUNNING,TIMEOUT,MEMOUT,INTERRUPTED};
extern interrupt_t execution_state;

#include <iostream>
#include <sstream>
class FullExpressionAccumulator {
public:
	std::streambuf* rdbuf(std::streambuf*);
	FullExpressionAccumulator(std::streambuf *);
	FullExpressionAccumulator(std::ostream& os);
	~FullExpressionAccumulator();
	void flush();
	void weak_flush();

    std::stringstream ss;
	std::ostream os;
private:
	boost::mutex shared_cout_mutex;
};

template <class T> FullExpressionAccumulator& operator<<(FullExpressionAccumulator& p, T const& t) {
	if(p.os.rdbuf()) 
	{
		p.ss << t; // accumulate into a non-shared stringstream, no threading issues
#ifdef _DEBUG
		p.flush();
#endif
	}
	return p;
}

template<typename Ty, typename Os>
void copy_deref_range(Ty first, Ty last, Os& out, char dbeg = '{', char delim = ',', char dend = '}'){
   out << dbeg;
   while (first != last) {
      out << **first; //note: only for containers that store pointers
      if (++first != last) 
		  out << delim;
   }
   out << dend;
}

#endif