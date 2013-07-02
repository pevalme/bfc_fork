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

#include <map>
#include <string>
typedef std::map<std::string,unsigned> scmap_t;

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

#define COUNT_IF(c,i,b) ((b && ++c[i]),b)

//shared data
#include <iostream>

#include <boost/thread.hpp>
extern boost::mutex fwbw_mutex;

enum graph_type_t {GTYPE_TIKZ,GTYPE_DOT,GTYPE_NONE};
extern graph_type_t graph_type, tree_type;

enum interrupt_t { RUNNING,TIMEOUT,MEMOUT,INTERRUPTED};
extern interrupt_t exe_state;

#include <iostream>
#include <sstream>
class ostream_sync {
public:
	std::streambuf* rdbuf(std::streambuf*);
	ostream_sync(std::streambuf *);
	ostream_sync(std::ostream& os);
	~ostream_sync();
	void flush();
	void weak_flush();
	void set_force_flush(bool);
	void swap(ostream_sync&);

    std::stringstream ss;
	std::ostream os;

	static bool force_flush;
private:
	static boost::mutex shared_cout_mutex;
};

template <class T> 
ostream_sync& operator<<(ostream_sync& p, T const& t) {
	if(p.os.rdbuf()) 
	{
		p.ss << t; // accumulate into a non-shared stringstream, no threading issues
		if(p.force_flush) p.flush();
	}
	return p;
}

typedef std::ostream& (*ostream_manipulator)(std::ostream&);
ostream_sync& operator<<(ostream_sync& os, ostream_manipulator pf);

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

enum po_rel_t
{
	neq_le = 0,		//covered by
	eq = 1,			//equal
	neq_ge = 2,		//covers
	neq_nge_nle	= 3,//incomparable

	//combinations
	neq_ge__or__neq_nge_nle = 4,
	neq_le__or__neq_nge_nle = 5,
	neq_ge__or__neq_le = 6
};

struct BState;
typedef BState const* bstate_t;
typedef BState* bstate_nc_t;

typedef std::set<bstate_t>	s_scpc_t;
struct insert_t
{
	po_rel_t	case_type;
	bstate_t	new_el; //pointer to element from M
	s_scpc_t	ex; //pointers to elements from states (neq_ge) or suspended_states (neq_le)

	insert_t(po_rel_t c, bstate_t i, s_scpc_t il);
	insert_t(po_rel_t c, bstate_t i, bstate_t il);
	insert_t();
};

#include <boost/functional/hash.hpp>
template <typename T>
struct container_hash {
    std::size_t operator()(T const& c) const {
        return boost::hash_range(c.begin(), c.end());
    }
};

std::string add_leading_zeros(const std::string& s, unsigned count);

std::istream& safeGetline(std::istream&,std::string&);

#endif