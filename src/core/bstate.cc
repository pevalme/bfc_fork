/******************************************************************************
  Synopsis		[State object for greedy algorithm.]

  Author		[Alexander Kaiser]

(C) 2011 Alexander Kaiser, University of Oxford, United Kingdom

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

#include "bstate.h"

#include "multiset_adapt.h"
#ifndef TEST
#include "antichain.h"
#endif

#include <boost/functional/hash.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

using namespace std;
using namespace boost;

std::size_t vec_husher::operator()(const vector<local_t>& v) const 
{
	size_t seed = 0;
	boost::hash_combine(seed, v.size());
	foreach(const local_t& l, v){
		boost::hash_combine(seed, l);
	}
	return seed;
}

bool vec_eqstr::operator()(const vector<local_t>& v1, const vector<local_t>& v2) const 
{
	invariant(is_sorted(v1.begin(),v1.end()));
	invariant(is_sorted(v2.begin(),v2.end()));

	unsigned v1_sz = v1.size();
	if(v1_sz != v2.size()) return 0;
	for(unsigned i = v1_sz-1; i != 0u-1; --i){
		if(v1[i] != v2[i]) return 0;
	}
	return 1;
}

/* ---- Priority queue---- */
bool priority_iterator_comparison::operator() (Bpriority_iterator& lhs, Bpriority_iterator& rhs) const 
{
	return (lhs.second < rhs.second);
}

BState::neighborhood_t::neighborhood_t(bstate_t source, neighborhood_t::pre_set_t predecessor, neighborhood_t::suc_set_t successors, status_t m, bool sleeps)
	: ini(false), src(source), pre(predecessor), suc(successors), status(m), sleeping(sleeps), depth(0), gdepth(-1)
{
}


#ifdef USE_SETS
BState::blocking_t::blocking_t(): blocked_by(), blocks()
#else
BState::blocking_t::blocking_t(): blocked_by(INIT_BUCKETS), blocks(INIT_BUCKETS)
#endif
{
}

BState::~BState()
{
	//invariant(fl == 0);
#ifndef TEST
	if(us != nullptr && nb != nullptr && nb->ini)
		delete us; //note: "us" is usually shared between multiple BState objects
#endif
	if(nb != nullptr)
		delete nb;
	if(bl != nullptr)
		delete bl;
}

/* ---- Constructors ---- */
BState::BState(shared_t s, bool alloc, type_t t)
	: type(t), shared(s), nb(alloc?new neighborhood_t():nullptr), bl(alloc?new blocking_t():nullptr), us(nullptr), fl(0)
{
}

BState::BState(string str, bool alloc)
	: nb(nullptr), bl(nullptr), us(nullptr), fl(0)
{

	try
	{
		vector<string> SL, B;
		split( SL, str, is_any_of("|"));
		if(SL.size() != 2) throw logic_error("separator missing");
		this->shared = lexical_cast<shared_t>(SL[0]);
		split(B, SL[1], is_any_of(",") ), for_each(B.begin(), B.end(), [this](string b){ bounded_locals.insert(lexical_cast<local_t>(b)); });
		type = def;
	}
	catch(...)
	{
		//throw runtime_error((string("invalid state string: ") + str).c_str()); 
		type = invalid;
	}

	if(alloc)
		nb = new neighborhood_t(), bl = new blocking_t();

}

void BState::allocate()
{
	invariant(nb == nullptr);
	invariant(bl == nullptr);

	nb = new neighborhood_t;
	bl = new blocking_t;
}

void BState::deallocate()
{
	if(nb != nullptr)
		delete nb;
	if(bl != nullptr)
		delete bl;
}

/* ---- Order operators ---- */
bool BState::operator == (const BState& r) const
{
	return 
		(this->shared == r.shared) && 
		(this->bounded_locals.size() == r.bounded_locals.size()) &&
		(this->bounded_locals == r.bounded_locals);
}

bool BState::operator <= (const BState& r) const
{
	invariant(this->type == BState::def || this->type != r.type);
	if(this->type == BState::bot || r.type == BState::top) 
		return 1;
	else if(this->type == BState::top || r.type == BState::bot || this->shared != r.shared)
		return 0;
	else
		return this->bounded_locals <= r.bounded_locals;
}

bool BState::operator <= (const OState& r) const
{ 
	if(this->shared != r.shared)
		return 0;
	else
		return leq(
		this->bounded_locals.begin(), 
		this->bounded_locals.end(), 
		r.bounded_locals.begin(), 
		r.bounded_locals.end(), 
		r.unbounded_locals.end(), 
		r.unbounded_locals.end(), 
		r.unbounded_locals.begin(), 
		r.unbounded_locals.end());
}

/* ---- Output operators ---- */
ostream& BState::operator << (ostream& out) const
{
	if(!out.rdbuf()) return out; //otherwise this causes a significant performace overhead

	switch(type){
	case BState::top:
		{
			out << "top";
			break;
		}
	case BState::def:
		{
			out << this->shared;
			string sep = "|"; foreach(local_t l, this->bounded_locals){ out << sep << l; sep = ","; }
			break;
		}
	case BState::bot:
		{
			out << "invalid";
			break;
		}
	}

	return out;
}

string BState::id_str() const 
{
	string ret = 's' + boost::lexical_cast<string>(this->shared);
	foreach(local_t l, this->bounded_locals)
		ret += 'l' + boost::lexical_cast<string>(l);
	return ret;
}

string BState::str_latex() const 
{
	string l_part, sep;
	foreach(local_t l, set<local_t>(bounded_locals.begin(),bounded_locals.end()))
		for(unsigned i=1; i<=bounded_locals.count(l); ++i) //l_part += (sep + boost::lexical_cast<string>(l) + '^' + boost::lexical_cast<string>(bounded_locals.count(l))), sep = ',';
			l_part += (sep + "\\LOCALSTATE{" + boost::lexical_cast<string>(l) + "}"), sep = ',';

	return (string)"$" + "\\graphconf" +  '{' + "\\SHAREDSTATE{" + boost::lexical_cast<string>(shared) + '}' + '}' + '{' + l_part + '}' + '$';
}

ostream& BState::mindot(std::ostream& out, bool mark, string shape) const
{
	precondition(!(nb == nullptr));
	out << '"' << id_str() << '"' << ' ' << "[label=\"" << *this << " (" << this->nb->gdepth << "/" << this->size() << ")" << "\"," << "fontcolor=" << (mark?"orange":(nb->status == BState::pending?"red":"green")) << ",shape=\"" << shape << "\"]";
	return out;
}

/* ---- Misc ---- */
size_t BState::size() const
{
	return this->bounded_locals.size();
}

size_t BState::prio() const //0 is max
{
	invariant(nb != nullptr);
	if(nb->ini) return 0;
	else return bounded_locals.size();
}

void BState::swap(BState& other)
{
	std::swap(fl,other.fl); //not needed
	std::swap(us,other.us); //not needed
	std::swap(type,other.type);
	std::swap(shared, other.shared);
	bounded_locals.swap(other.bounded_locals);
	std::swap(nb, other.nb);
}

bool BState::consistent(check_t c) const
{
	if(S == 0 || (shared >= S && shared != invalid_shared) ) return 0;
	foreach(const local_t& l, bounded_locals) if(l >= L) return 0;

	if(c == partial_check) return 1;

	switch(type){
	case BState::top: break;
	case BState::def: break;
	case BState::bot: break;
	default: return 0;
	}

	switch(nb->status){
	case BState::blocked_processed: break;
	case BState::blocked_pending: break;
	case BState::pending: break;
	case BState::processed: break;
	case BState::unset: break;
	default: return 0;
	}

	foreach(const bstate_t& f,bl->blocks) 
		if(f == nullptr)
			return 0;

	return 1;
}

size_t BState::L;
size_t BState::S;

/* ---- Helper for output operator ---- */
ostream& operator << (ostream& out, const BState& t)
{ 
	return t.operator<<(out);
}

/* ---- Helpers for unordered containers ---- */
std::size_t husher::operator()(const BState& p) const 
{
	size_t seed = 0;
	boost::hash_combine(seed, p.shared);
	for(bounded_t::const_iterator l = p.bounded_locals.begin(), le = p.bounded_locals.end(); l != le; ++l)
		boost::hash_combine(seed, *l);
	return seed;
}

bool eqstr::operator()(const BState& s1, const BState& s2) const
{ 
	return 
		s1.shared == s2.shared 
		&& s1.bounded_locals.size() == s2.bounded_locals.size() 
		&& s1.bounded_locals == s2.bounded_locals;
}

std::size_t husher_p::operator()(BState const *const& p) const
{
	static husher h;
	return h(*p);
}

bool eqstr_p::operator()(BState const *const& s1, BState const *const& s2) const
{
	static eqstr e;
	return e(*s1,*s2);
}

/********************************************************
containment graph
********************************************************/
#include <stack>

//note: return false exactly if a x was not blocked before
void dequeue(BState const* x)
{
	for(auto pi = x->bl->blocked_by.begin(), pe = x->bl->blocked_by.end(); pi != pe;){
		BState const * p = *pi;
		(*pi)->bl->blocks.erase(x), x->bl->blocked_by.erase(pi++);

		for(auto qi = x->bl->blocks.begin(), qe = x->bl->blocks.end(); qi != qe;){
			BState const * q = *qi;
			if(pi == pe)
				(*qi)->bl->blocked_by.erase(x), x->bl->blocks.erase(qi++);
			else
				qi++;
			
			bool connected = false;
			stack<bstate_t> C; 
			foreach(bstate_t i, p->bl->blocks) 
				C.push(i);
			
			while(!C.empty() && !connected){
				bstate_t c = C.top(); C.pop();
				
				for(auto i = c->bl->blocks.begin(), ie = c->bl->blocks.end(); i != ie; ++i){
					if(*i == q){
						connected = true;
						break;
					}else if(!(**i <= *x) && !(*x <= **i)){
						//x and i incomparable
						C.push(*i);
					}
				}
			}

			if(!connected)
				p->bl->blocks.insert(q), q->bl->blocked_by.insert(p);
		}
	}
}

bool enqueue(const set<bstate_t>& bots, const set<bstate_t>& tops, BState const* x)
{
	set<BState const *> MaxLE;
	{	
		stack<BState const *> C; 
		foreach(bstate_t bot, bots) C.push(bot);
		
		while(!C.empty()){
			BState const * c = C.top(); C.pop();

			invariant(!(*c == *x));
			bool maximal = true;
			foreach(BState const * n, c->bl->blocks)
			{
				invariant(!(*n == *x));
				if(*n <= *x) 
					C.push(n), maximal = false;
			}

			if(maximal) MaxLE.insert(c);
		}
	}

	set<BState const *> MinGE;
	{	
		stack<BState const *> C; 
		foreach(bstate_t top, tops) C.push(top);
		while(!C.empty()){
			BState const * c = C.top(); C.pop();

			invariant(!(*c == *x));
			bool minimal = true;
			foreach(BState const * n, c->bl->blocked_by)
			{
				invariant(!(*n == *x));
				if(*x <= *n) 
					C.push(n), minimal = false;
			}

			if(minimal) MinGE.insert(c);
		}
	}

	foreach(BState const * lt, MaxLE){
		foreach(BState const * gt, MinGE){
			lt->bl->blocks.erase(gt);
			gt->bl->blocked_by.erase(lt);
		}
	}

	foreach(BState const * lt, MaxLE){
		lt->bl->blocks.insert(x), x->bl->blocked_by.insert(lt);
	}

	bool blocks_non_sleeping = false;
	foreach(BState const * gt, MinGE){
		blocks_non_sleeping |= !gt->nb->sleeping;
		gt->bl->blocked_by.insert(x), x->bl->blocks.insert(gt);
	}

	return blocks_non_sleeping;
}

bool enqueue(bstate_t& bot, const set<bstate_t>& tops, BState const* x)
{
	set<bstate_t> bots;
	bots.insert(bot);
	return enqueue(bots,tops,x);
}

pair<BState const *, bool> enqueue(BState const *bot, BState const *top, BState const* x)
{
	
	set<BState const *> MaxLE;
	{	
		stack<BState const *> C; C.push(bot);
		while(!C.empty()){
			BState const * c = C.top(); C.pop();

			invariant(!(*c == *x));
			bool maximal = true;
			foreach(BState const * n, c->bl->blocks)
				if(*n == *x) return pair<BState const *, bool>(n,false);
				else if(*n <= *x) C.push(n), maximal = false;

			if(maximal) MaxLE.insert(c);
		}
	}

	set<BState const *> MinGE;
	{	
		stack<BState const *> C; C.push(top);
		while(!C.empty()){
			BState const * c = C.top(); C.pop();

			invariant(!(*c == *x));
			bool minimal = true;
			foreach(BState const * n, c->bl->blocked_by)
				if(*n == *x) return pair<BState const *, bool>(n,false);
				else if(*x <= *n) C.push(n), minimal = false;

			if(minimal) MinGE.insert(c);
		}
	}

	foreach(BState const * lt, MaxLE){
		foreach(BState const * gt, MinGE){
			lt->bl->blocks.erase(gt);
			gt->bl->blocked_by.erase(lt);
		}
	}

	foreach(BState const * lt, MaxLE){
		lt->bl->blocks.insert(x), x->bl->blocked_by.insert(lt);
	}
	foreach(BState const * gt, MinGE){
		gt->bl->blocked_by.insert(x), x->bl->blocks.insert(gt);
	}

#ifdef WIN32
	return make_pair(nullptr,true);
#else
	return make_pair((BState const *)nullptr,true);
#endif
}
