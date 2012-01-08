#ifndef TYPES_H
#define TYPES_H

#ifndef WIN32
#define nullptr 0
#endif

#include "multiset_adapt.h"

#include <boost/foreach.hpp>
#define foreach(a,b) BOOST_FOREACH(a,b)

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
extern std::ostream inf;
extern std::ostream fwinfo;
extern std::ostream preinf;
extern std::ostream bout;
extern std::ostream dout;
extern std::ostream statsout;

#include <boost/thread.hpp>
extern boost::mutex shared_cout_mutex;


enum graph_type_t {GTYPE_TIKZ,GTYPE_DOT,GTYPE_NONE};
extern graph_type_t graph_type;

enum interrupt_t { RUNNING,TIMEOUT,MEMOUT,UNKNOWN};
extern interrupt_t execution_state;
#endif