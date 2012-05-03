/******************************************************************************
  Synopsis		[Wrapper for antichain data structure.]

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

#include "antichain.h"

#include "net.h"
#include "vstate.h"
#include "bstate.h"


#include <stack>

using namespace std;

antichain_t::CG_t::CG_t(CG_t&& other)
{
	this->bot = other.bot, other.bot = nullptr;
	this->top = other.top, other.top = nullptr;
	this->nodes.swap(other.nodes);
}

antichain_t::CG_t::CG_t(): bot(new VState(VState::bot)), top(new VState(VState::top))
{
}

antichain_t::CG_t::~CG_t()
{
	delete bot;
	delete top;
}

void antichain_t::CG_t::clear()
{
	nodes.clear();
	bot->bl->blocks__set.clear(), top->bl->blocked_by__set.clear();
	bot->bl->blocks__set.insert(top), top->bl->blocked_by__set.insert(bot);
}

void antichain_t::CG_t::swap(CG_t& other)
{
	std::swap(bot,other.bot);
	std::swap(top,other.top);
	nodes.swap(other.nodes);
}

antichain_t::CG_t& antichain_t::CG_t::operator=(const CG_t& rhs)
{
	invariant(rhs.nodes.empty());
	return *this;
}

antichain_t::antichain_t(bool ownership_): ownership(ownership_), G()
{
	invariant(BState::L != 0); 
	G.bot->bl->blocks__set.insert(G.top), G.top->bl->blocked_by__set.insert(G.bot);
}

antichain_t::antichain_t(antichain_t&& other)
{
	this->G.swap(other.G);
	this->M.swap(other.M);
	this->ownership = other.ownership, other.ownership = false;
}

antichain_t::~antichain_t()
{
	if(!ownership)
		return;
	
	foreach(bstate_t b, M)
		delete b;
}

antichain_t& antichain_t::operator=(const antichain_t &rhs)
{
	M = rhs.M;
	G = rhs.G;

	return *this;
}

antichain_t::insert_t::insert_t(po_rel_t c, bstate_t i, s_scpc_t il): case_type(c), new_el(i), exist_els(il)
{
}

antichain_t::insert_t::insert_t(po_rel_t c, bstate_t i, bstate_t il): case_type(c), new_el(i), exist_els()
{
	exist_els.insert(il); 
}

antichain_t::insert_t::insert_t()
{
}

antichain_t::insert_t antichain_t::case_insert(bstate_t s)
{
	//note: s must be globally accessible, as it is only referenced in M
	invariant(s->consistent());
	invariant(s->bounded_locals.size() != 0);

	Breached_p_t::iterator f = M.find(s);
	if(f != M.end()) 
		//case: eq -- s is exactly contained in M ("match")
		return insert_t(eq,nullptr,*f); //note: return f, not s here!

	s_scpc_t le = LGE(s,less_equal);
	if(!le.empty())
		//case: neq_ge -- s is already contained in us(M), though not exactly ("overcut")
		return insert_t(neq_ge,nullptr,le);

	s_scpc_t lg = LGE(s,greater_equal);
	//clean look-up table and M set (non-minimal elements are removed)
	invariant(M.find(s) == M.end());
	foreach(bstate_t const& x, lg) erase(x);

	//update look-up table and M set
	integrate(s);

	//cases: neq_le, neq_nge_nle
	return insert_t((lg.empty()?(neq_nge_nle):(neq_le)), s, lg);
}

antichain_t::insert_t antichain_t::max_case_insert(bstate_t s)
{
	//note: s must be globally accessible, as it is only referenced in M
	invariant(s->consistent());

	Breached_p_t::iterator f = M.find(s);
	if(f != M.end())
		//case: eq -- s is exactly contained in M ("match")
		return antichain_t::insert_t(eq,nullptr,*f); //note: return f, not s here!

	s_scpc_t lg = LGE(s,greater_equal);
	if(!lg.empty())
		//case: neq_le -- s is already contained in ls(M), though not exactly ("undercut")
		return antichain_t::insert_t(neq_le,nullptr,lg);

	s_scpc_t le = LGE(s,less_equal);
	//clean look-up table and M set (non-maximal elements are removed)
	foreach(bstate_t const& x, le) erase(x);

	//update look-up table and M set
	integrate(s);

	//cases: neq_le, neq_nge_nle
	return antichain_t::insert_t((lg.empty()?(neq_nge_nle):(neq_ge)), s, le);
}

po_rel_t antichain_t::relation(BState const * s)
{
	Breached_p_t::iterator f = M.find(s);
	if(f != M.end()) return eq;

	s_scpc_t le = LGE(s,less_equal);
	if(!le.empty()) return neq_ge;

	s_scpc_t lg = LGE(s,greater_equal);
	if(!lg.empty()) return neq_le;

	return neq_nge_nle;
}

void antichain_t::insert_incomparable(BState const * s)
{
	invariant(M.find(s) == M.end());
	invariant(LGE(s,less_equal).empty());
	invariant(LGE(s,greater_equal).empty());
	integrate(s);
}

antichain_t::s_scpc_t antichain_t::insert_neq_le(BState const * s)
{
	invariant(M.find(s) == M.end()); //the element does not yet exist
	invariant(LGE(s,less_equal).empty()); //no smaller elements exists
	invariant(!LGE(s,greater_equal).empty()); //there exists at least one larger element
	
	//clean look-up table and M set (non-minimal elements are removed)
	s_scpc_t lg = LGE(s,greater_equal);
	foreach(bstate_t const& x, lg) erase(x);

	//update look-up table and M set
	integrate(s);

	return lg;
}
/*
antichain_t::insert_t antichain_t::case_insert(bstate_t s)
{
	//note: s must be globally accessible, as it is only referenced in M
	invariant(s->consistent());
	invariant(s->bounded_locals.size() != 0);

	Breached_p_t::iterator f = M.find(s);
	if(f != M.end()) 
		//case: eq -- s is exactly contained in M ("match")
		return insert_t(eq,nullptr,*f); //note: return f, not s here!

	s_scpc_t le = LGE(s,less_equal);
	if(!le.empty())
		//case: neq_ge -- s is already contained in us(M), though not exactly ("overcut")
		return insert_t(neq_ge,nullptr,le);

	s_scpc_t lg = LGE(s,greater_equal);
	//clean look-up table and M set (non-minimal elements are removed)
	invariant(M.find(s) == M.end());
	foreach(bstate_t const& x, lg) erase(x);

	//update look-up table and M set
	integrate(s);

	//cases: neq_le, neq_nge_nle
	return insert_t((lg.empty()?(neq_nge_nle):(neq_le)), s, lg);
}
*/




pair<bstate_t, bool> antichain_t::insert(BState const * s)
{
	insert_t r(case_insert(s));

	return make_pair(r.new_el,r.case_type == neq_nge_nle || r.case_type == neq_le);
}

pair<bstate_t, bool> antichain_t::max_insert(BState const * s)
{
	insert_t r(max_case_insert(s));

	return make_pair(r.new_el,r.case_type == neq_nge_nle || r.case_type == neq_ge);
}

//description: clean look-up table and M set
void antichain_t::erase(BState const * x)
{
	invariant(manages(x));

	M.erase(x);

	VState decomp = decompose(x);
	us_VState_t::iterator f = G.nodes.find(decomp);
	invariant(f != G.nodes.end());

	invariant(f != G.nodes.end());
	invariant(f->bl->suc.empty() || !f->bl->suc.empty());

	f->bl->suc.erase(x); //here

	if(f->bl->suc.empty()){
		f->dequeue();
		G.nodes.erase(f);
	}

	G.bot->bl->suc.erase(x);

}

void antichain_t::clear()
{
	M.clear();
	G.clear();
}

bool antichain_t::manages(BState const * s) const
{
	//note: this will check whether a state y in M exsits with *y = *s
	return M.find(s) != M.end();
}

const Breached_p_t& antichain_t::M_cref() const
{
	return M;
}

//description: update look-up table and M set
void antichain_t::integrate(BState const * x)
{

	invariant(!manages(x));

	VState decomp = decompose(x);

	us_VState_t::iterator f = G.nodes.find(decomp);
	if(f != G.nodes.end()){
		//subset already exists; add
		f->bl->suc.insert(x);
	}else{
		pair<us_VState_t::iterator, bool> ins = G.nodes.insert(move(decomp)); //the bl pointer in decomp must be preserved
		ins.first->bl->suc.insert(x);
		ins.first->enqueue(G.bot,G.top);
	}

	M.insert(x);
}

VState antichain_t::decompose(bstate_t p) const
{
	return VState(p->bounded_locals.begin(),p->bounded_locals.end());
}

antichain_t::s_scpc_t antichain_t::LGE(bstate_t p, const order_t order)
{

	//decompose s
	VState decomp = decompose(p); //performance: could be done at call-site

	s_scpc_t isec;

	us_VState_t::const_iterator pi = G.nodes.find(decomp);
	if(pi == G.nodes.end()){
		stack<VState const *> C;
		switch(order){
		case less_equal: 
			C.push(G.bot);
			break;
		case greater_equal: 
			C.push(G.top);
			break;
		}

		switch(order){
		case less_equal: 

			while(!C.empty()){
				VState const * c = C.top(); 
				C.pop();

				if(c == G.bot){

					for(auto m = c->bl->blocks__set.begin(), me = c->bl->blocks__set.end(); m != me; ++m){
						if(*m != G.top && (*m)->bounded_locals <= decomp.bounded_locals) 
							C.push(*m); 
						for(unordered_set<bstate_t>::const_iterator i = c->bl->suc.begin(), ie = c->bl->suc.end(); i != ie; ++i){
							if((*i)->bounded_locals <= p->bounded_locals) isec.insert(*i);
						}
					}

				}else{

					for(auto m = c->bl->blocks__set.begin(), me = c->bl->blocks__set.end(); m != me; ++m){
						if(*m != G.top && (*m)->bounded_locals <= decomp.bounded_locals) 
							C.push(*m); 
						for(unordered_set<bstate_t>::const_iterator i = c->bl->suc.begin(), ie = c->bl->suc.end(); i != ie; ++i){
							if((*i)->bounded_locals <= p->bounded_locals) isec.insert(*i);
						}
					}

				}
			}
			break;
		case greater_equal: 
			while(!C.empty()){
				VState const * c = C.top(); 
				C.pop();

				if(c == G.top){

					for(auto m = c->bl->blocked_by__set.begin(), me = c->bl->blocked_by__set.end(); m != me; ++m){
						if(*m != G.bot && decomp.bounded_locals <= (*m)->bounded_locals) 
							C.push(*m);
						for(unordered_set<bstate_t>::const_iterator i = c->bl->suc.begin(), ie = c->bl->suc.end(); i != ie; ++i){
							if(p->bounded_locals <= (*i)->bounded_locals) isec.insert(*i);
						}
					}
				}
				else{
					for(auto m = c->bl->blocked_by__set.begin(), me = c->bl->blocked_by__set.end(); m != me; ++m){
						if(*m != G.bot && decomp.bounded_locals <= (*m)->bounded_locals) 
							C.push(*m);
						for(unordered_set<bstate_t>::const_iterator i = c->bl->suc.begin(), ie = c->bl->suc.end(); i != ie; ++i){
							if(p->bounded_locals <= (*i)->bounded_locals) isec.insert(*i);
						}
					}
				}
			}
			break;
		}
	}else{
		stack<VState const *> C;
		C.push(&(*pi));

		switch(order){
		case less_equal: 
			while(!C.empty()){
				VState const * c = C.top(); 
				C.pop();

				for(auto m = c->bl->blocked_by__set.begin(), me = c->bl->blocked_by__set.end(); m != me; ++m){
					if(*m != G.bot){
						invariant((*m)->bounded_locals <= decomp.bounded_locals);
						C.push(*m); 
					}

					for(unordered_set<bstate_t>::const_iterator i = c->bl->suc.begin(), ie = c->bl->suc.end(); i != ie; ++i){
						if((*i)->bounded_locals <= p->bounded_locals) isec.insert(*i);
					}
				}
			}
			break;
		case greater_equal: 
			while(!C.empty()){
				VState const * c = C.top(); 
				C.pop();

				for(auto m = c->bl->blocks__set.begin(), me = c->bl->blocks__set.end(); m != me; ++m){
					if(*m != G.top){
						invariant(decomp.bounded_locals <= (*m)->bounded_locals);
						C.push(*m);
					}

					for(unordered_set<bstate_t>::const_iterator i = c->bl->suc.begin(), ie = c->bl->suc.end(); i != ie; ++i){
						if(p->bounded_locals <= (*i)->bounded_locals) isec.insert(*i);
					}
				}
			}
			break;
		}
	}

	return isec;

}

#include "net.h"
extern Net net;

vec_antichain_t::vec_antichain_t(bool ownership)//: uv(0)
{
	for(unsigned i = 0; i < BState::S; ++i)
		if(net.core_shared(i))
			//uv.insert(move(antichain_t(ownership)));
			uv[i] = move(antichain_t(ownership));

	//auto garabge = antichain_t(ownership);
	//for(unsigned i = 0; i < BState::S; ++i)
	//	if(net.core_shared(i))
	//		uv.push_back(move(antichain_t(ownership)));
	//	else
	//		uv.push_back(move(garabge));
}

vec_antichain_t::~vec_antichain_t()
{
}

size_t vec_antichain_t::size() const
{
	size_t sz = 0;
	//foreach(const antichain_t& s, uv)
	//	sz += s.M_cref().size();
	foreachit(t, uv)
		sz += t->second.M_cref().size();

	return sz;
}

size_t vec_antichain_t::graph_size() const
{
	size_t sz = 0;
	//foreach(const antichain_t& s, uv)
	//	sz += s.G.nodes.size();
	foreachit(t, uv)
		sz += t->second.G.nodes.size();

	return sz;
}

antichain_t::insert_t vec_antichain_t::case_insert(bstate_t s)
{ 
	return uv[s->shared].case_insert(s); 
}

pair<bstate_t, bool> vec_antichain_t::insert(BState const * s, bool safe_free)
{ 
	return uv[s->shared].insert(s); 
}

antichain_t::insert_t vec_antichain_t::max_case_insert(bstate_t s)
{ 
	return uv[s->shared].max_case_insert(s); 
}

pair<bstate_t, bool> vec_antichain_t::max_insert(BState const * s, bool safe_free){ 
	return uv[s->shared].max_insert(s); 
}

set<bstate_t> vec_antichain_t::LGE(BState const * s, antichain_t::order_t order)
{ 
	return uv[s->shared].LGE(s,order); 
}

void vec_antichain_t::insert_incomparable(BState const * s)
{ 
	uv[s->shared].insert_incomparable(s); 
}

antichain_t::s_scpc_t vec_antichain_t::insert_neq_le(BState const * s)
{ 
	return uv[s->shared].insert_neq_le(s); 
}

void vec_antichain_t::erase(BState const * s)
{ 
	uv[s->shared].erase(s); //here
}

bool vec_antichain_t::manages(BState const * s)
{ 
	return uv[s->shared].manages(s); 
}

const Breached_p_t& vec_antichain_t::M_cref(shared_t shared) const
{
	invariant(uv.find(shared) != uv.end());
	//return uv[shared].M_cref(); 
	return uv.find(shared)->second.M_cref();
}

po_rel_t vec_antichain_t::relation(BState const * s)
{ 
	return uv[s->shared].relation(s); 
}
