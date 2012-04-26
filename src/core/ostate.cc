/******************************************************************************
  Synopsis		[State object for the modified Karp-Miller procedure.]

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

#include "ostate.h"

#include <iostream>

#include "multiset_adapt.h"

#include <boost/foreach.hpp>
#include <boost/tokenizer.hpp>
#include <boost/functional/hash.hpp>
#include <boost/lexical_cast.hpp>

#include "combination.hpp"

using namespace std;

bool Opriority_iterator_comparison_less::operator() (Opriority_iterator& lhs, Opriority_iterator& rhs) const
{
	return (lhs.second < rhs.second);
}

bool Opriority_iterator_comparison_greater::operator() (Opriority_iterator& lhs, Opriority_iterator& rhs) const
{
	return (lhs.second > rhs.second);
}

size_t OState::L;
size_t OState::S;

/* ---- Constructors ---- */
OState::OState(shared_t s): shared(s), accel(nullptr), prede(nullptr), tsucc(false), depth(0)
{
}

OState::OState(istream& cin): accel(nullptr), prede(nullptr), tsucc(false), depth(0)
{
	cout << "Enter shared initial: "; cin >> shared;

	string blocals, ulocals;
	cout << "Enter bounded locals (comma sep.) or 'none': "; cin >> blocals;
	if(blocals != "none"){
		boost::tokenizer<boost::char_separator<char> > tok(blocals, boost::char_separator<char>(","));
		for(boost::tokenizer<boost::char_separator<char> >::iterator i = tok.begin(); i != tok.end(); ++i) 
			bounded_locals.insert(boost::lexical_cast<local_t>(*i));
	}

	cout << "Enter unbounded locals (comma sep.) or 'none': "; cin >> ulocals;
	if(ulocals != "none"){
		boost::tokenizer<boost::char_separator<char> > tok2(ulocals, boost::char_separator<char>(","));
		for(boost::tokenizer<boost::char_separator<char> >::iterator i = tok2.begin(); i != tok2.end(); ++i) 
			unbounded_locals.insert(boost::lexical_cast<local_t>(*i));
	}
}

//vector<OState> OState::resolve_omegas(size_t max_width) const
//{
//	vector<OState> ret;
//
//	vector<local_t> bench(max_width*unbounded_locals.size());
//	merge_mult(bounded_locals.begin(), bounded_locals.begin(), unbounded_locals.begin(), unbounded_locals.end(), max_width, bench.begin());
//
//	for(unsigned r = 1; r <= max_width; ++r){
//		do {
//			//vector<local_t> prj(bench.begin(), bench.begin() + r);
//			OState t(shared,bounded_locals.begin(),bounded_locals.end()); //ignore unbounded components
//			t.bounded_locals.insert(bench.begin(), bench.begin() + r);
//			ret.push_back(t);
//		} while (boost::next_combination(bench.begin(), bench.begin() + r, bench.begin() + bench.size()));
//	}
//
//	return ret;
//}

Oreached_t OState::resolve_omegas(size_t max_width) const
{
	precondition(!unbounded_locals.empty());

	Oreached_t ret;

	vector<local_t> bench(max_width*unbounded_locals.size());
	merge_mult(bounded_locals.begin(), bounded_locals.begin(), unbounded_locals.begin(), unbounded_locals.end(), max_width, bench.begin());

	for(unsigned r = 1; r <= max_width; ++r){
		do {
			OState* t = new OState(shared,bounded_locals.begin(),bounded_locals.end()); //ignore unbounded components
			t->bounded_locals.insert(bench.begin(), bench.begin() + r);

			if(!ret.insert(t).second)
				delete t;

		} while (boost::next_combination(bench.begin(), bench.begin() + r, bench.begin() + bench.size()));
	}

	postcondition_foreach(ostate_t r, ret, this != r);
	postcondition_foreach(ostate_t r, ret, r->unbounded_locals.empty()); //all omegas are resolved
	postcondition_foreach(ostate_t r, ret, *r<=*this && !(*r == *this)); //*this strictly covers all returned elements

	return ret;
}

/* ---- Order operators ---- */
bool OState::operator == (const OState& r) const
{
	const OState& l = *this;
	return 
		(l.shared == r.shared) && 
		(l.bounded_locals.size() == r.bounded_locals.size()) &&
		(l.unbounded_locals.size() == r.unbounded_locals.size()) &&
		(l.bounded_locals == r.bounded_locals) && 
		(l.unbounded_locals == r.unbounded_locals);
}

//note: this function is significantly fasten than leq__SPEC, e.g. for one test-suite 1.5s vs 7s
bool OState::operator <= (const OState& r) const
{ 
	if(this->shared != r.shared) 
		return 0;
	else
		return leq(
		this->bounded_locals.begin(), 
		this->bounded_locals.end(), 
		r.bounded_locals.begin(), 
		r.bounded_locals.end(), 
		this->unbounded_locals.begin(),
		this->unbounded_locals.end(), 
		r.unbounded_locals.begin(), 
		r.unbounded_locals.end());
}

/* ---- Misc ---- */
void OState::swap(OState& other)
{
	std::swap(shared, other.shared);
	bounded_locals.swap(other.bounded_locals);
	unbounded_locals.swap(other.unbounded_locals);
	std::swap(prede, other.prede);
}

size_t OState::size() const
{
	return bounded_locals.size() + unbounded_locals.size();
}

bool OState::consistent() const
{
	if(S == 0 || (shared >= S && shared != invalid_shared) ) return 0;
	foreach(const local_t& l, bounded_locals) if(l >= L || unbounded_locals.find(l) != unbounded_locals.end()) return 0;
	foreach(const local_t& l, unbounded_locals) if(l >= L || bounded_locals.find(l) != bounded_locals.end()) return 0;
	return 1;
}

ostream& OState::operator << (ostream& out) const
{ 
	if(!out.rdbuf()) return out; //otherwise this causes a significant performace overhead

	out << this->shared;
	string sep = "|"; foreach(local_t l, this->bounded_locals){ out << sep << l; sep = ","; }
	sep = "/"; foreach(local_t l, this->unbounded_locals){ out << sep << l; sep = ","; }

	return out;
}

string OState::str_id() const
{
	string ret = 's' + boost::lexical_cast<string>(this->shared);
	foreach(local_t l, this->bounded_locals)
		ret += 'b' + boost::lexical_cast<string>(l);
	foreach(local_t l, this->unbounded_locals)
		ret += 'u' + boost::lexical_cast<string>(l);

	ret += "_d" + boost::lexical_cast<string>(depth);

	return ret;
}

string OState::str_latex() const
{

	set<local_t> locals(bounded_locals.begin(),bounded_locals.end());
	locals.insert(unbounded_locals.begin(),unbounded_locals.end());

	string l_part, sep;
	foreach(local_t l, locals)
		l_part += (sep + boost::lexical_cast<string>(l) + '^' + (bounded_locals.count(l)?boost::lexical_cast<string>(bounded_locals.count(l)):"\\omega")), sep = ',';

	return (string)"$" + "\\graphconf" +  '{' + boost::lexical_cast<string>(shared) + '}' + '{' + l_part + '}' + '$';
}

/* ---- Helper for output operator ---- */
ostream& operator << (ostream& out, const OState& t)
{ 
	return t.operator<<(out);
}

/* ---- Helpers for unordered containers ---- */
std::size_t OState_husher::operator()(const OState& p) const
{
	size_t seed = 0;
	boost::hash_combine(seed, p.shared);
	for(bounded_t::const_iterator bl = p.bounded_locals.begin(), ble = p.bounded_locals.end(); bl != ble; ++bl)
		boost::hash_combine(seed, *bl);
	for(unbounded_t::const_iterator ul = p.unbounded_locals.begin(), ule = p.unbounded_locals.end(); ul != ule; ++ul)
		boost::hash_combine(seed, *ul);
	return seed;
}

bool OState_eqstr::operator()(const OState& s1, const OState& s2) const
{ 
	return 
		s1.shared == s2.shared && 
		s1.bounded_locals.size() == s2.bounded_locals.size() && 
		s1.unbounded_locals.size() == s2.unbounded_locals.size() && 
		s1.bounded_locals == s2.bounded_locals && 
		s1.unbounded_locals == s2.unbounded_locals;
}

std::size_t ostate_t_husher::operator()(ostate_t const& p) const
{
	OState_husher h;
	return h(*p);
}

bool ostate_t_eqstr::operator()(ostate_t const& s1, ostate_t const& s2) const
{ 
	OState_eqstr e;
	return e(*s1,*s2);
}
