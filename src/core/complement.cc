/******************************************************************************
  Synopsis		[Complement data structure.]

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

#include "complement.h"

#include "combination.hpp"
#include "bstate.h"

#include <stack>
#include <boost/thread.hpp>

using namespace std;

complement_set::~complement_set()
{
	foreach(cmb_node_p u, u_nodes)
		delete u;

	foreach(cmb_node_p m, m_nodes)
		delete m;

	foreach(cmb_node_p l, l_nodes)
		delete l;
}

complement_set::complement_set(unsigned l, unsigned k, bool f): L(l), K(k), full_sat(f), u_nodes(), m_nodes(), l_nodes() 
{ 
	if(k==0) return;
	
	for(local_t l = 0; l < L; ++l)
	{
		cmb_node* new_node = new cmb_node(1,l); //add all minimal elements
		u_nodes.insert(new_node);
	}
}

complement_set::complement_set(complement_set&& other)
{
	full_sat = other.full_sat;
	L = other.L, other.L = 0;
	K = other.K, other.K = 0;
	u_nodes.swap(other.u_nodes);
	m_nodes.swap(other.m_nodes);
	l_nodes.swap(other.l_nodes);
}

bool complement_set::insert(cmb_node& c) //call discipline: 0 must be inserted before 0,0 etc.
{
	us_cmb_node_p_t::iterator f = u_nodes.find(&c);
	invariant(!((u_nodes.find(&c) != u_nodes.end()) == (l_nodes.find(&c) != l_nodes.end()))); //safe if call discipline is met

	if(f == u_nodes.end()) 
		return false; //f is already in the lower set
	else{
		remove(*f);
		return true; //f is in the upper set and is therefore removed
	}
}

//note: the address of c is irrelevant
pair<bool,us_cmb_node_p_t> complement_set::diff_insert(const cmb_node& c) //call discipline: 0 must be inserted before 0,0 etc.
{
	us_cmb_node_p_t::const_iterator f = u_nodes.find(&c);
	invariant(!((u_nodes.find(&c) != u_nodes.end()) == (l_nodes.find(&c) != l_nodes.end()))); //safe if call discipline is met

	if(f == u_nodes.end()) 
		return make_pair(false,us_cmb_node_p_t()); //f is already in the lower set
	else 
		return make_pair(true,diff_remove(*f)); //f is in the upper set and is therefore removed
}

void complement_set::remove(cmb_node_p f)
{
	invariant(*(f->rc) == 0);

	try_expand_remove(f);
	foreach(cmb_node_p p, *(f->lt)){
		invariant(multiset<local_t>(f->c.begin(), f->c.end()) < multiset<local_t>(p->c.begin(), p->c.end()) && *(p->rc) > 0);
		if(--(*p->rc) == 0) m_nodes.erase(p), u_nodes.insert(p);
	}

	u_nodes.erase(f), l_nodes.insert(f);
}

us_cmb_node_p_t complement_set::diff_remove(cmb_node_p f)
{
	invariant(*(f->rc) == 0);

	us_cmb_node_p_t u_nodes_diff;

	diff_try_expand_remove(f,u_nodes_diff);
	foreach(cmb_node_p p, *(f->lt)){
		invariant(multiset<local_t>(f->c.begin(), f->c.end()) < multiset<local_t>(p->c.begin(), p->c.end()) && *(p->rc) > 0);
		if(--*(p->rc) == 0) 
			m_nodes.erase(p), u_nodes.insert(p), u_nodes_diff.insert(p);
	}

	u_nodes.erase(f), l_nodes.insert(f);

	return u_nodes_diff;
}

void complement_set::try_expand_remove(cmb_node_p f) 
{
	invariant(*(f->rc) == 0);

	if(!(f->c.size() < K)) return;

	local_t l;
	cmb_node* new_node;

	static stack<cmb_node*> work_set;
	
	local_t lp = *f->c.begin();
	for(l = full_sat?0:lp; l < (full_sat?L:lp+1); ++l){ //for(l = 0; l < L; ++l){
		new_node = new cmb_node(), new_node->c = f->c, new_node->c.push_back(l);
		sort(new_node->c.begin(), new_node->c.end()); //keep new_node->c sorted
		if(m_nodes.insert(new_node).second) work_set.push(new_node); //add new [c.push_back(l)] to the work list
		else delete new_node; //existing were already processed
	}

	while(!work_set.empty()){
		cmb_node_p cp = work_set.top(); work_set.pop();
		cmb_node::cmb_t v_int(cp->c);
		do {
			cmb_node t(v_int.begin(), --v_int.end(), 0, NULL);
			invariant(!((u_nodes.find(&t) != u_nodes.end()) == (m_nodes.find(&t) != m_nodes.end())));
			if(f->c == t.c) continue; 

			us_cmb_node_p_t::iterator q = u_nodes.find(&t);
			if(q == u_nodes.end()) 
				q = m_nodes.find(&t);
			(*q)->lt->push_back(cp); ++*(cp->rc);
		} while (boost::next_combination(v_int.begin(), --v_int.end(), v_int.end()));
		if(*(cp->rc) == 0) m_nodes.erase(cp), u_nodes.insert(cp);
	}

	return;
}

void complement_set::diff_try_expand_remove(cmb_node_p f,us_cmb_node_p_t& u_nodes_diff) 
{
	invariant((*f->rc) == 0);

	if(!(f->c.size() < K)) return;

	static local_t l;
	static cmb_node* new_node;
	static unsigned nex_sz;

	static stack<cmb_node*> work_set;

	local_t lp = *f->c.begin();
	for(l = full_sat?0:lp; l < (full_sat?L:lp+1); ++l){ //for(l = 0; l < L; ++l){
		new_node = new cmb_node(), new_node->c = f->c, new_node->c.push_back(l);
		sort(new_node->c.begin(), new_node->c.end()); //keep new_node->c sorted
		if(m_nodes.insert(new_node).second) work_set.push(new_node); //add new [c.push_back(l)] to the work list
		else delete new_node; //existing were already processed
	}

	while(!work_set.empty()){
		cmb_node* cp = work_set.top(); work_set.pop();
		cmb_node::cmb_t v_int(cp->c);
		do {
			cmb_node t(v_int.begin(), --v_int.end(), 0, NULL);
			invariant(!((u_nodes.find(&t) != u_nodes.end()) == (m_nodes.find(&t) != m_nodes.end())));
			if(f->c == t.c) continue; 

			us_cmb_node_p_t::iterator q = u_nodes.find(&t);
			if(q == u_nodes.end()) 
				q = m_nodes.find(&t);
			(*q)->lt->push_back(cp); ++*(cp->rc);
		} while (boost::next_combination(v_int.begin(), --v_int.end(), v_int.end()));
		if(*(cp->rc) == 0) m_nodes.erase(cp), u_nodes.insert(cp), u_nodes_diff.insert(cp);
	}

	return;
}

complement_vec::complement_vec(unsigned k, unsigned s, unsigned l, bool f): K(k), S(s), L(l), full_sat(f) //note: luv(s, complement_set(l,k)) in the initializer list does not work, since all shared states will then get the same nodes
{ 
	for(unsigned i = 0; i < s; ++i)
		luv.push_back(move(complement_set(l,k,f)));
}

//complement_vec::complement_vec(complement_vec&& other)
//{ 
//	std::swap(K,other.K);
//	std::swap(S,other.S);
//	std::swap(L,other.L);
//	luv.swap(other.luv);
//}

pair<bool,us_cmb_node_p_t> complement_vec::diff_insert(shared_t s, const cmb_node& c)
{
	return luv[s].diff_insert(c); 
}

void complement_vec::clear()
{
	luv.clear();
	for(unsigned i = 0; i < S; ++i)
		luv.push_back(move(complement_set(L,K,full_sat)));
}

size_t complement_vec::lower_size()
{
	size_t ret = 0;
	for(unsigned s = 0; s < BState::S; ++s)
		ret += luv[s].l_nodes.size();

	return ret;
}

size_t complement_vec::upper_size()
{
	size_t ret = 0;
	for(unsigned s = 0; s < BState::S; ++s)
		ret += luv[s].u_nodes.size();

	return ret;
}

void complement_vec::print_lower_set(ostream& out) const
{ 
	for(unsigned s = 0; s < BState::S; ++s)
	{
		foreach(const cmb_node_p c, luv[s].u_nodes)
			out << BState(s,c->c.begin(),c->c.end()) << endl;
	}
}

void complement_vec::print_upper_set(ostream& out) const
{ 
	for(unsigned s = 0; s < BState::S; ++s)
	{
		foreach(const cmb_node_p c, luv[s].l_nodes)
			out << BState(s,c->c.begin(),c->c.end()) << endl;
	}
}

/* ---- Projection ---- */	
unsigned complement_vec::project_and_insert(const OState& g)
{

	if(this->K == 0) return 0;

	unsigned new_projections = 0;

	//note: these can be reused for all calls
	static vector<cmb_node> prj; //one for each differen k to prevent reallocation
	static vector<local_t> gg2; 

	unsigned gg2_n;

	gg2_n = g.bounded_locals.size() + this->K * g.unbounded_locals.size();
	if(gg2_n == 0) return false; //todo: what is this for?

	if(gg2.size() < gg2_n) gg2.resize(gg2_n, invalid_local);
	merge_mult(g.bounded_locals.begin(), g.bounded_locals.end(), g.unbounded_locals.begin(), g.unbounded_locals.end(), this->K, gg2.begin());

	for(unsigned i = prj.size() + 1; i <= this->K; ++i) //for a give k, this loop is executed only once
		prj.push_back(cmb_node(i)); 

	for(unsigned r = 1; r <= this->K && r <= gg2.size(); ++r){
		do {
			invariant(r <= gg2.size());
			invariant(is_sorted(gg2.begin(), gg2.begin() + r));
			copy(gg2.begin(), gg2.begin() + r, prj[r-1].c.begin());
			if(this->luv[g.shared].insert(prj[r-1])) ++new_projections;
		} while (boost::next_combination(gg2.begin(), gg2.begin() + r, gg2.begin() + gg2_n));
	}

	return new_projections;
}

lowerset_vec::lowerset_vec(unsigned k, unsigned s, bool prj_all, bool f): K(k), lv(s), full_sat(f)
{
}

lowerset_vec::~lowerset_vec()
{
	foreach(us_cmb_node_p_t& uset, this->lv)
		foreach(cmb_node_p p, uset)
			delete p;
}

void lowerset_vec::clear()
{
	foreach(us_cmb_node_p_t& uset, this->lv)
		foreach(cmb_node_p p, uset)
			delete p;
}

//note: this is currently copied from the class complement_vec
unsigned lowerset_vec::project_and_insert(const OState& g, shared_cmb_deque_t& shared_cmb_deque, bool forward_projections)
{
	unsigned new_projections = 0;

	//note: these can be reused for all calls
	static vector<cmb_node> prj; //one for each differen k to prevent reallocation
	static vector<local_t> gg2; 

	unsigned gg2_n;

	gg2_n = g.bounded_locals.size() + K * g.unbounded_locals.size();
	if(gg2_n == 0) return false;

	if(gg2.size() < gg2_n) gg2.resize(gg2_n, invalid_local);
	merge_mult(g.bounded_locals.begin(), g.bounded_locals.end(), g.unbounded_locals.begin(), g.unbounded_locals.end(), K, gg2.begin());

	for(unsigned i = prj.size() + 1; i <= K; ++i) //for a give k, this loop is executed only once
		prj.push_back(cmb_node(i)); 

	list<pair<shared_t,cmb_node_p> > projections;

	for(unsigned r = 1; r <= min(K,gg2_n); ++r){ //the latter loop condition catches the case when we try to project a state that has less components than k: (0|0) cannot be projected with k=2
		do {
			invariant(is_sorted(gg2.begin(), gg2.begin() + r));
			copy(gg2.begin(), gg2.begin() + r, prj[r-1].c.begin());

			if(this->lv[g.shared].find(&prj[r-1]) == this->lv[g.shared].end())
			{
				us_cmb_node_p_t::const_iterator i = this->lv[g.shared].insert(new cmb_node(prj[r-1],false)).first;
				++new_projections;
				if(forward_projections)
					projections.push_back(make_pair(g.shared,*i));
			}

		} while (boost::next_combination(gg2.begin(), gg2.begin() + r, gg2.begin() + gg2_n));
	}

	if(!projections.empty())
	{
		shared_cmb_deque.mtx.lock();
		shared_cmb_deque.lst->splice(shared_cmb_deque.lst->end(),projections);
		shared_cmb_deque.mtx.unlock();
	}

	return new_projections;
}


size_t lowerset_vec::size() const
{
	size_t ret = 0;

	foreach(const us_cmb_node_p_t& v, lv)
		ret += v.size();

	return ret;
}
