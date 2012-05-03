/******************************************************************************
  Synopsis		[Node objects for antichain data structures.]

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

#include "vstate.h"

#include <stack>
#include "types.h"

using namespace std;

po_rel_t cmp__leq_eq(const vector<local_t>& l, const vector<local_t>& r)
{
	vector<local_t>::const_iterator first1, last1, first2, last2;

	first1	= l.begin();
	last1	= l.end();
	first2	= r.begin();
	last2	= r.end();

	bool eq_ = true;
	for(;first1 != last1 && first2 != last2;){
		if(*first1 < *first2)
			return neq_ge__or__neq_nge_nle;
		else if(*first1 > *first2)
			++first2, eq_ = false;
		else
			++first1, ++first2;
	}

	if(first1 == last1){
		if(eq_ && first2 == last2) 
			return eq;
		else 
			return neq_le;
	}else{
		return neq_ge__or__neq_nge_nle;
	}
}

bool operator <= (const vector<local_t>& l, const vector<local_t>& r)
{
	vector<local_t>::const_iterator first1, last1, first2, last2;

	first1	= l.begin();
	last1	= l.end();
	first2	= r.begin();
	last2	= r.end();

	for(;first1 != last1 && first2 != last2;){
		if(*first1 < *first2)
			return false;
		else if(*first1 > *first2)
			++first2;
		else
			++first1, ++first2;
	}

	return first1 == last1;
}

//whether l and r are incomparable, i.e. !(l <= r)&&!(r <= l); hence !(x == y) is different from (x != y)
bool operator != (const vector<local_t>& l, const vector<local_t>& r)
{
	invariant(!(l == r));
	vector<local_t>::const_iterator first1, last1, first2, last2;

	first1	= l.begin();
	last1	= l.end();
	first2	= r.begin();
	last2	= r.end();

	po_rel_t res = neq_ge__or__neq_le;
	for(;first1 != last1 && first2 != last2;){
		if(*first1 < *first2){
			if(res == neq_ge__or__neq_nge_nle){
				invariant(!(l <= r) && !(r <= l));
				return true;
			}else 
				res = neq_le__or__neq_nge_nle, ++first1;
		}
		else if(*first2 < *first1)
			if(res == neq_le__or__neq_nge_nle){
				invariant(!(l <= r) && !(r <= l));
				return true;
			}else 
				res = neq_ge__or__neq_nge_nle, ++first2;
		else
			++first1, ++first2;
	}

	if(res == neq_ge__or__neq_le){
		invariant(!(!(l <= r) && !(r <= l)));
		return false; //both are comparable
	}else if(res == neq_le__or__neq_nge_nle){
		invariant((!(l <= r) && !(r <= l)) == (first2 != last2));
		return first2 != last2;
	}else{
		invariant((!(l <= r) && !(r <= l)) == (first1 != last1));
		return first1 != last1;
	}

}

VState::VState(VState&& other)
{
	this->bounded_locals.swap(other.bounded_locals);
	type = other.type;
	bl = other.bl, other.bl = nullptr;
}

VState::VState(type_t t): type(t), bl(new blocking_t)
{
}

VState::~VState()
{
	if(bl != nullptr)
		delete bl;
}

bool operator == (const vector<local_t>& l, const vector<local_t>& r)
{
	if(l.size() != r.size()) return false;

	vector<local_t>::const_iterator first1 = l.begin();
	vector<local_t>::const_iterator last1 = l.end();
	vector<local_t>::const_iterator first2 = r.begin();

	for(;first1 != last1; ++first1, ++first2){
		if(*first1 != *first2) 
			return false;
	}

	return true;
}

bool VState::operator == (const VState& r) const 
{ 
	if((this->type == VState::top && r.type == VState::top) || (this->type == VState::bot && r.type == VState::bot)) 
		return true;

	return this->bounded_locals == r.bounded_locals;
}

bool VState::operator <= (const VState& r) const 
{ 
	if(r.type == VState::top || this->type == VState::bot) return true;
	else if(this->type == VState::top || r.type == VState::bot) return false;
	else if(this->bounded_locals.size() > r.bounded_locals.size()) return false;
	return this->bounded_locals <= r.bounded_locals;
}

pair<VState const *, bool> VState::enqueue(VState const *bot, VState const *top) const
{

	set<VState const *> MaxLE;
	{	
		stack<VState const *> C; C.push(bot);
		while(!C.empty()){
			VState const * c = C.top(); C.pop();

			invariant(!(*c == *this));
			bool maximal = true;
			for(unordered_set<VState const*>::const_iterator n = c->bl->blocks__set.begin(), ne = c->bl->blocks__set.end(); n != ne; ++n){
				invariant((*n)->consistent());

				if((*n)->type == VState::top)
					continue;
				else if((*n)->bounded_locals <= this->bounded_locals){
					C.push(*n), maximal = false;
				}
				invariant(!(**n == *this)); //this case should be handled in advance using a hash set
			}

			if(maximal) MaxLE.insert(c);
		}
	}

	set<VState const *> MinGE;
	{	
		stack<VState const *> C; C.push(top);
		while(!C.empty()){
			VState const * c = C.top(); C.pop();

			invariant(!(*c == *this));
			bool minimal = true;
			for(unordered_set<VState const*>::const_iterator n = c->bl->blocked_by__set.begin(), ne = c->bl->blocked_by__set.end(); n != ne; ++n){

				if((*n)->type == VState::bot)
					continue;
				else if(this->bounded_locals <= (*n)->bounded_locals){
					C.push(*n), minimal = false;
				}
				invariant(!(**n == *this)); //this case should be handled in advance using a hash set
			}

			if(minimal) MinGE.insert(c);
		}
	}

	foreach(VState const * lt, MaxLE){
		foreach(VState const * gt, MinGE){
			lt->bl->blocks__set.erase(gt);
			gt->bl->blocked_by__set.erase(lt);
		}
	}

	foreach(VState const * lt, MaxLE){
		lt->bl->blocks__set.insert(this), this->bl->blocked_by__set.insert(lt);
	}
	foreach(VState const * gt, MinGE){
		gt->bl->blocked_by__set.insert(this), this->bl->blocks__set.insert(gt);
	}

#ifdef WIN32
	return make_pair(nullptr,true);
#else
	return make_pair((VState const *)nullptr,true);
#endif
}

void VState::dequeue() const
{

	unordered_set<VState const*>::const_iterator qi,qe,i,ie;
	for(auto pi = this->bl->blocked_by__set.begin(), pe = this->bl->blocked_by__set.end(); pi != pe;){
		VState const * p = *pi;
		(*pi)->bl->blocks__set.erase(this), this->bl->blocked_by__set.erase(pi++);

		for(qi = this->bl->blocks__set.begin(), qe = this->bl->blocks__set.end(); qi != qe;){
			const VState* q = *qi;
			if(pi == pe)
				(*qi)->bl->blocked_by__set.erase(this), this->bl->blocks__set.erase(qi++);
			else
				qi++;

			bool connected = false;
			stack<const VState*> C;
			foreach(const VState* i, p->bl->blocks__set) 
				C.push(i);

			while(!C.empty() && !connected){
				const VState* c = C.top(); C.pop();

				for(i = c->bl->blocks__set.begin(), ie = c->bl->blocks__set.end(); i != ie; ++i){
					if(*i == q){
						connected = true;
						break;
					}
				}

				if(connected) break;

				for(i = c->bl->blocks__set.begin(), ie = c->bl->blocks__set.end(); i != ie; ++i){
					if( (*i)->type != VState::top && (*i)->bounded_locals != this->bounded_locals){
						//both are incomparabel
						C.push(*i);
					}
				}
			}

			if(!connected)
				p->bl->blocks__set.insert(q), q->bl->blocked_by__set.insert(p);
		}
	}
}

bool VState::consistent() const
{
	if(type != bot && type != def && type != top) return false;
	if(bounded_locals.size() > BState::L) return false;

	return true;
}

std::size_t VState_husher::operator()(const VState& v) const
{
	return vh.operator()(v.bounded_locals);
}

bool VState_eqstr::operator()(const VState& v1, const VState& v2) const
{
	return ve.operator()(v1.bounded_locals,v2.bounded_locals);
}
