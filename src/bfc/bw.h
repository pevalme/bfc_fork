/******************************************************************************
  Synopsis		[Greedy Algorithm for Multi-Threaded Programs 
				with Non-Blocking Communication.]

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

//BUG(??): e.g. for plainpn.ttd and cbfs -f .\ex_plainpn\plainpn.ttd -sgraph -bw -k 1 -target "2|0,0,0,1,1,1,2" one get's a=1|2,2 as query (its predecessors two are stored) though a is no query and a predecessor of 2|2, hence a's predecessor should be discarded since they cover 2|2. Hence, the graph becomes larger than neccessary. 
#ifndef BW_H
#define BW_H

#include "antichain.h"
#include "multiset_adapt.h"
#include "options_str.h"
#include "unordered_priority_set.h"

#include <stack>

/********************************************************
Misc
********************************************************/
typedef unordered_priority_set<bstate_t> work_pq;
typedef set<bstate_t> W_deferred_t;
typedef Breached_p_t non_minimals_t;
typedef work_pq pending_t;
typedef bstate_t bstate_t;
typedef complement_vec complement_vec_t;

unordered_priority_set<bstate_t>::keyprio_type keyprio_pair(bstate_t s)
{ 
	//invariant(bw_weight == s->nb->depth || bw_weight == s->size());

	switch(bw_weight)
	{
	case order_width: 
		return make_pair(s,s->size());
	case order_depth: 
		return make_pair(s,s->nb->depth);
	default:	
		return make_pair(s,0);
	}
}

string add_leading_zeros(const string& s, unsigned count)
{
	int diff = count-s.size();
	if(diff > 0) return string(diff,'0') + s;
	else return s;
}

template<typename Ty>
void copy_deref_range(Ty first, Ty last, ostream& out = cout, char dbeg = '{', char delim = ',', char dend = '}'){
   out << dbeg;
   while (first != last) {
      out << **first; //note: only for containers that store pointers
      if (++first != last) 
		  out << delim;
   }
   out << dend;
}

#include "bw.inv.h"
#include "bw.out.h"

/********************************************************
Pre image
********************************************************/
Breached_p_t Pre(const BState& s, Net& n)
{
	Breached_p_t pres; //set of successors discovered for s so far (a set is used to avoid reprocessing)

	stack<const BState*> work; work.push(&s); //todo: allocate nb and bl here already?

	while(!work.empty())
	{
		
		const BState* cur = work.top(); work.pop();

		//bool porable = false;
		//foreach(local_t l,cur->bounded_locals)
		//{
		//	if(!n.core_local(l))
		//	{
		//		porable = true;
		//		break;
		//	}
		//}

		if(cur != &s && n.core_shared(cur->shared)/* && !porable*/)
		{
			if(!pres.insert(cur).second)
				delete cur;
			
			continue;
		}

		invariant(iff(cur == &s, n.core_shared(cur->shared)));

		auto predecs_it = n.adjacency_list_tos2.find(cur->shared);
		if(predecs_it == n.adjacency_list_tos2.end()) 
		{
			preinf << "Ignore: No incoming transition" << endl;
			if(cur != &s) 
				delete cur;
			
			continue; //no incoming transitions
		}
		
		for(auto t = predecs_it->second.begin(), te = predecs_it->second.end(); t != te; ++t)
		{

			const Thread_State& u = t->source;
			const Thread_State& v = t->target; 
			assert(cur->shared == v.shared);

			//debug_assert(implies(cur != &s && n.core_shared(cur->shared),porable));
			//if(cur != &s && n.core_shared(cur->shared) && n.core_local(v.local))
			//	continue; //only execute por-related transitions

			const bool thread_in_u = (cur->bounded_locals.find(u.local) != cur->bounded_locals.end());
			const bool thread_in_v = (cur->bounded_locals.find(v.local) != cur->bounded_locals.end());
			const bool horiz_trans = (u.shared == v.shared);
			const bool diff_locals = (u.local != v.local);
			const trans_type&	ty = t->type;

			preinf << *t << endl;

			debug_assert(!(ty == spawn_transition && !thread_in_u && thread_in_v && !horiz_trans && !diff_locals));
			debug_assert(!(ty == spawn_transition && thread_in_u && !thread_in_v && !horiz_trans && !diff_locals));
			debug_assert(!(ty == spawn_transition && !thread_in_u && thread_in_v && horiz_trans && !diff_locals));
			debug_assert(!(ty == spawn_transition && thread_in_u && !thread_in_v && horiz_trans && !diff_locals));

			if((horiz_trans && !thread_in_v) || (thread_in_u && diff_locals && ty == transfer_transition) || (horiz_trans && !diff_locals && thread_in_u)) //last condition encodes self-loops
			{
				preinf << "Ignore: not smaller/incomparable predecessor resp. no predecessor" << endl;
				continue;
			}

			BState* pre = new BState(horiz_trans?cur->shared:u.shared,cur->bounded_locals.begin(),cur->bounded_locals.end(),false); //allocation

			if(thread_in_v && diff_locals)
			{
				if(ty == thread_transition || ty == spawn_transition)
				{ //replace one
					pre->bounded_locals.erase(pre->bounded_locals.find(v.local));
					if(ty == thread_transition || !thread_in_u)
						pre->bounded_locals.insert(u.local);
				}
				else
				{ //replace all
					bounded_t::const_iterator v_local;
					while((v_local = pre->bounded_locals.find(v.local)) != pre->bounded_locals.end())
					{
						bstate_t n = new BState(*pre); //allocation
						work.push(n); 
						pre->bounded_locals.erase(v_local); //remove exaclty one
						pre->bounded_locals.insert(u.local);
					}
				}
			}
			else if(!thread_in_u && ty == spawn_transition)
			{
				pre->bounded_locals.insert(u.local);
			}
			else if(!thread_in_v && ty == thread_transition)
			{ //add u.l
				pre->bounded_locals.insert(u.local); //note: transfer transitions are non-blocking, why no u.local must be added
			}

			work.push(pre);

		}

		if(cur != &s) 
			delete cur;

	}

	return pres;

}

/********************************************************
Pruning
********************************************************/
void global_clean_up(bstate_t& s, vec_antichain_t& M, non_minimals_t& N, vec_antichain_t& O, pending_t& W, W_deferred_t& W_deferred)
{

	precondition(iff(s->nb->status == BState::pending && !s->nb->sleeping, W.contains(keyprio_pair(s)) || W_deferred.find(s) != W_deferred.end())); //pending states are in the worklist
	precondition(iff(s->nb->status == BState::pending || s->nb->status == BState::processed, M.manages(s))); //pending and processed states are minimal
	precondition(iff(s->nb->status == BState::blocked_pending || s->nb->status == BState::blocked_processed, N.find(s) != N.end())); //blocked states are not minimal
	precondition(iff(s->bl->blocks.empty() && (s->nb->status == BState::blocked_pending || s->nb->status == BState::blocked_processed), O.manages(s))); //blocked states that do not block others are maximal
	
	//not checked
	precondition(implies(!s->nb->sleeping,iff(s->nb->status == BState::pending && !W.contains(keyprio_pair(s)), W_deferred.find(s) != W_deferred.end())));
	
	if(s->nb->status == BState::pending && !s->nb->sleeping)
	{
		if(W.contains(keyprio_pair(s)))
			W.erase(keyprio_pair(s));
		else
			W_deferred.erase(s);
	}

	if(s->nb->status == BState::pending || s->nb->status == BState::processed)
		M.erase(s);

	if(s->nb->status == BState::blocked_pending || s->nb->status == BState::blocked_processed)
	{
		N.erase(s);

		if(s->bl->blocks.empty())
		{
			O.erase(s);

			foreach(bstate_t b, s->bl->blocked_by)
				if((b->nb->status == BState::blocked_processed || b->nb->status == BState::blocked_pending) && b->bl->blocks.size() == 1)
					O.insert_incomparable(b);
		}
	}

	postcondition(!W.contains(keyprio_pair(s)));
	postcondition(W_deferred.find(s) == W_deferred.end());
	postcondition(!M.manages(s));
	postcondition(N.find(s) == N.end());
	postcondition(!O.manages(s));

}

void wake_up(bstate_t& s, vec_antichain_t& M, non_minimals_t& N, vec_antichain_t& O, pending_t& W)
{

	precondition(implies(s->nb->src->us != nullptr,!s->nb->src->us->manages(s)));
	precondition(!W.contains(keyprio_pair(s)));
	precondition(!M.manages(s));
	precondition(N.find(s) == N.end());

	precondition_foreach(bstate_t b, s->bl->blocks, implies(b->type != BState::top, b->nb->status == BState::blocked_pending || b->nb->status == BState::blocked_processed));
	precondition_foreach(bstate_t b, s->bl->blocks, implies(b->type != BState::top, N.find(b) != N.end()));
	precondition_foreach(bstate_t b, s->bl->blocks, implies(b->type != BState::top, implies(b->nb->status == BState::blocked_pending, b->nb->suc.empty())));
	precondition_foreach(bstate_t b, s->bl->blocks, implies(b->type != BState::top, !W.contains(keyprio_pair(b))));

	auto i = s->bl->blocks.begin(), 
		e = s->bl->blocks.end();

	for(;i != e;)
	{
		bstate_t b = *i++;

		s->bl->blocks.erase(b), b->bl->blocked_by.erase(s); //remove blocking edges between s and b

		if(b->bl->blocked_by.empty()) //check whether b is still blocked by other states
		{
			invariant(M.relation(b) == neq_nge_nle);
			invariant(b->nb->status == BState::blocked_pending || b->nb->status == BState::blocked_processed);

			M.insert(b), N.erase(b); //b is now minimal; move it from the set of non-minimal elements to the minimal antichain
			invariant(implies(!b->nb->ini, iff(b->bl->blocks.empty(), O.manages(b))));
			if(O.manages(b))
				O.erase(b);

			switch(b->nb->status){ //adjust its status and add it to the workset if it needs to be processed
			case BState::blocked_pending: 
				b->nb->status = BState::pending;
				if(!b->nb->sleeping)
				{
					bout << "wake up " << *b << endl;
					W.push(keyprio_pair(b));
				}
				else
				{
					bout << "do not wake up " << *b << " (it's sleeping)" << endl;
				}
				break;
			case BState::blocked_processed: b->nb->status = BState::processed; break;}
		}
	}

	postcondition(s->bl->blocks.empty());

}

//capture the direct tree starting in s with s as the new source
void local_capture(bstate_t s)
{

	stack<bstate_t> Q; 
	Q.push(s);

	bstate_t p;
	while(!Q.empty())
	{
		p = Q.top(), Q.pop();

		invariant(s != p->nb->src); //before p is changed, its source is not equal to s
		invariant(p->nb->src->us->manages(p));
		invariant(s->us->relation(p) == neq_nge_nle); //p is guaranteed to be incomparable wrt. to all other locally minimal elements

		foreach(bstate_t suc, p->nb->suc)
			if(suc->nb->src == p->nb->src) Q.push(suc); //traverse all successors with the same source

		if(p == s) s->us->insert(p); //s still has a predecessor with the old source
		else s->us->insert(p), p->nb->src->us->erase(p); //otherwise move it from the old to the new local upperset

		p->nb->src = s; //update the source

		invariant(s == p->nb->src);
		invariant(s->us->manages(p));
	}

}

unsigned prune(bstate_t s, vec_antichain_t& M, non_minimals_t& N, vec_antichain_t& O, pending_t& W, complement_vec_t& C, vec_antichain_t& D, bstate_t target, W_deferred_t& W_deferred)
{

	precondition_foreach(bstate_t pre, s->nb->pre, pre->nb->src != s->nb->src); //s must not have predecessors with the same source; holds for source states and must also hold for non-souce states that are locally undercut (in the latter case without this assumption it is possible that a locally non-minimal states is erased when an (also) locally non-minimal predecessor state is detach prior the it)
	precondition(implies(s->nb->src->us != nullptr,!s->nb->src->us->manages(s))); //special case, s must be local-erased before prune is called (this makes it easier in the case of local pruning, where the minimal states are already locally M-inserted when the function prune is called
	
	stack<bstate_t> Q;
	Q.push(s);

	unsigned pruned_ctr = 0;
	bool dealloc_s = false;
	bstate_t p;
	while(!Q.empty())
	{

		p = Q.top(), Q.pop();

		invariant(implies(p->nb->src->us != nullptr,iff(p != s, p->nb->src->us->manages(p))));
		invariant_foreach(bstate_t suc, p->nb->suc, implies(p->nb->src->us != nullptr,p->nb->src->us->manages(suc)));
		invariant(p->nb->status == BState::pending || p->nb->status == BState::processed || p->nb->status == BState::blocked_pending || p->nb->status == BState::blocked_processed);
		//invariant(iff(p->nb->status == BState::pending && !p->nb->sleeping, W.contains(keyprio_pair(p)))); //pending states are in the worklist

		invariant(iff(p->nb->status == BState::pending && !p->nb->sleeping, W.contains(keyprio_pair(p)) || W_deferred.find(p) != W_deferred.end())); //pending states are in the worklist

		invariant(iff(p->nb->status == BState::pending || p->nb->status == BState::processed, M.manages(p))); //pending and processed states are minimal
		invariant(iff(p->nb->status == BState::blocked_pending || p->nb->status == BState::blocked_processed, N.find(p) != N.end())); //blocked states are not minimal

		auto i = p->nb->suc.begin(), 
			e = p->nb->suc.end();

		for(;i != e;)
		{
			bstate_t suc = *i++;
			if(suc->nb->src == p->nb->src) Q.push(suc); //only remember direct successors
			else p->nb->src->us->erase(suc); //but locally erase indirect successors

			p->nb->suc.erase(suc), suc->nb->pre.erase(p); //erase also indirect successors
		}

#ifdef EAGER_ALLOC
		if(p != s)
#else
		if(p != s && p->nb->src->us != nullptr)
#endif
			p->nb->src->us->erase(p);

		invariant(implies(p->nb->src->us != nullptr,!p->nb->src->us->manages(p))); //s is not in the local minimal antichain

		if(!p->nb->pre.empty()) //check whether there exists a predecessor with a different source
		{
			bstate_t new_src = (*p->nb->pre.begin())->nb->src; //select a new source

			switch(p->nb->status){ //adjust the status
			case BState::processed: p->nb->status = BState::pending, W.push(keyprio_pair(p)); break;
			case BState::blocked_processed: p->nb->status = BState::blocked_pending; break;}

			invariant(new_src->us->manages(p));

			p->nb->src = new_src;
		}
		else
		{
			//without C.K != 0 regression\spawn_vf_02 fails with option -k0 --mode B
			//the target is a source with potentially more threads than parameter k

			if(C.K != 0 && p->size() <= C.K && p->nb->ini) //in case a source with width <k is removed, new source states need to be added
			{
				foreach(const cmb_node_p n, C.diff_insert(p->shared,cmb_node(p->bounded_locals.begin(),p->bounded_locals.end(),0,nullptr)).second) //traverse all new minimal elements
				{
					auto t = new BState(p->shared,n->c.begin(),n->c.end(),true);
					auto exist = N.insert(t);
					if(exist.second) //check whether an existing state equals the new source state, and if yes, adopt it together with its search tree
					{
						t->nb->src = t, t->nb->ini = true, t->nb->status = BState::blocked_pending;
#ifdef EAGER_ALLOC
						t->us = new BState::vec_upperset_t, t->us->insert(t);
#else
						t->us = nullptr;
#endif
						bool blocks_non_sleeping = enqueue(p,O.LGE(t,vec_antichain_t::set_t::greater_equal),t);
						t->nb->sleeping = !(target == nullptr || *t <= *target) && !blocks_non_sleeping;
					}
					else
					{
						delete t;
						t = const_cast<BState*>(*exist.first);
						invariant(implies(t == target, t->nb->suc.empty()));
						invariant(implies(t == target, t->nb->ini));
						invariant(implies(t == target, t->us != nullptr && t->us->manages(t)));
						if(t != target)
						{
							t->nb->ini = true;
							release_assert(t->us == nullptr); //added: 7:06 PM 1/9/2012
							t->us = new BState::vec_upperset_t;
							local_capture(t);
						}
					}
				}
			}

			global_clean_up(p,M,N,O,W,W_deferred);

			switch(p->nb->status)
			{
			case BState::processed: case BState::pending: 
				wake_up(p,M,N,O,W); 
				break;
			case BState::blocked_processed: case BState::blocked_pending: 
				dequeue(p); 
				break;
			}

			invariant(p->bl->blocked_by.empty() && p->bl->blocks.empty());

			if(D.M_cref(p->shared).find(p) == D.M_cref(p->shared).end())
			{
				if(s == p)
					dealloc_s = true; //the upperset of a source needs to be accessible in order to process its successor in the succeeding loop iterations
				else
					delete p, ++pruned_ctr;
			}

		}
	}

	if(dealloc_s) //in case the first state was not reassociated to a new source it must now be deallocated; otherwise it is now part of a different source tree
		delete s, ++pruned_ctr;

	return pruned_ctr;

}

unsigned local_detach(bstate_t& expl_src, const set<bstate_t>& S, vec_antichain_t& M, non_minimals_t& N, vec_antichain_t& O, pending_t& W, complement_vec_t& C, vec_antichain_t& D, bstate_t t, W_deferred_t& W_deferred) //note: D is not used
{

	unsigned pruned_ctr = 0;

	foreach(const bstate_t& s, S)
	{
		auto pb = s->nb->pre.begin(), 
			pe = s->nb->pre.end();

		for(;pb != pe;){
			bstate_t p = *pb++;
			if(expl_src == p->nb->src)
				s->nb->pre.erase(p), p->nb->suc.erase(s); //detach s from its local search tree
		}
	}

	foreach(const bstate_t& s, S)
	{
		//debug_assert(expl_src != s->nb->src); //local subtree consolidation not supported
		
		if(expl_src == s->nb->src)
			pruned_ctr += prune(s,M,N,O,W,C,D,t,W_deferred);
	}

	return pruned_ctr;

}

set<bstate_t> sources(bstate_t s)
{

	set<bstate_t> ret;
	stack<bstate_t> W; 
	unordered_set<bstate_t> W_seen;

	W.push(s), W_seen.insert(s);
	while(!W.empty()){
		bstate_t w = W.top(); W.pop();
		if(w == w->nb->src) ret.insert(w);

		foreach(bstate_t p, w->nb->pre)
			if(W_seen.insert(p).second)
				W.push(p);
	}

	return ret;

}

set<bstate_t> sources(bstate_t s, vec_antichain_t& D)
{

	set<bstate_t> ret;
	stack<bstate_t> W; 
	unordered_set<bstate_t> W_seen;

	W.push(s), W_seen.insert(s);
	while(!W.empty()){
		bstate_t w = W.top(); W.pop();

		debug_assert(iff(w == w->nb->src, w->nb->ini));

		if(w == w->nb->src) 
			ret.insert(w);
		else 
			D.max_insert(w);

		foreach(bstate_t p, w->nb->pre)
			if(W_seen.insert(p).second)
				W.push(p);
	}

	return ret;

}

void try_prune(list<std::pair<shared_t, cmb_node_p> >& prunes, vec_antichain_t& M, non_minimals_t& N, vec_antichain_t& O, pending_t& W, complement_vec_t& C, vec_antichain_t& D, bstate_t t, unsigned& piteration, unsigned& ctr_prune_dequeues, unsigned& ctr_known_dequeues, W_deferred_t& W_deferred)
{
	for(auto get = prunes.begin(), e = prunes.end(); get != e; ++get)
	{
		BState to_be_pruned(get->first,get->second->c.begin(),get->second->c.end(),false);
		non_minimals_t::const_iterator f = M.M_cref(to_be_pruned.shared).find(&to_be_pruned);
		if(f == M.M_cref(to_be_pruned.shared).end())
		{
			++ctr_known_dequeues;
			continue;
		}
		else
		{
			bout << "--------------- Prune set iteration (F) " << piteration << " ---------------" << endl;
			bout << "prune " << to_be_pruned << endl;
			invariant(before_prune(*f,M,N,O,W,C));
#ifdef EAGER_ALLOC
			(*f)->us->erase(*f), prune(*f,M,N,O,W,C,D,t),piteration++,ctr_prune_dequeues++; //function prune has the precondition that s is not in its local upperset							
#else
			if((*f)->us != nullptr)
				(*f)->us->erase(*f);

			prune(*f,M,N,O,W,C,D,t,W_deferred),piteration++,ctr_prune_dequeues++; //function prune has the precondition that s is not in its local upperset							
#endif
			invariant(consistent(M,N,O,W,C)); //consistency check
		}
	}

	invariant(intersection_free(D,M));
}

bool try_prune(bstate_t& w, vec_antichain_t& M, non_minimals_t& N, vec_antichain_t& O, pending_t& W, complement_vec_t& C, vec_antichain_t& D, bstate_t t, unsigned& piteration, unsigned& ctr_prune_dequeues, W_deferred_t& W_deferred)
{
	bool found_target = false;
	invariant(intersection_free(D,M));
	foreach(bstate_t p, sources(w, D))
	{

		if(p == t)
			found_target = true;

		//if(graph_type != GTYPE_NONE) print_dot_search_graph(M,N,O,p,work_pq(),true,witeration,piteration,"_interm");

		bout << "--------------- Prune set iteration (B) " << piteration << " ---------------" << endl;
		bout << "prune " << *p << endl;
		invariant(before_prune(p,M,N,O,W,C));
		p->us->erase(p), prune(p,M,N,O,W,C,D,t,W_deferred), ++piteration; //function prune has the precondition that s is not in its local upperset
		invariant(consistent(M,N,O,W,C)); //consistency check
	}

	return !found_target;
}

/********************************************************
On-the-fly statistics
********************************************************/
unsigned 
	ctr_state_new = 0, ctr_state_not_new = 0, 
	ctr_locally_non_minimal = 0, ctr_locally_non_minimal__neq_ge = 0, ctr_locally_non_minimal__neq_ge__exists_max = 0, 
	ctr_locally_non_minimal__eq = 0, 
	ctr_locally_minimal = 0, ctr_locally_minimal__neq_le = 0, ctr_locally_minimal__neq_le__exists_max = 0,
	ctr_locally_minimal__neq_nge_nle = 0,
	ctr_globally_non_minimal = 0, ctr_globally_non_minimal__neq_ge = 0, ctr_globally_non_minimal__neq_ge__exists_max = 0, 
	ctr_globally_minimal = 0, ctr_globally_minimal__neq_le = 0, ctr_globally_minimal__neq_le__exists_max = 0,
	ctr_globally_minimal__neq_nge_nle = 0
	;

unsigned
	ctr_bw_maxdepth = 0,
	ctr_bw_maxwidth = 0,
	ctr_bw_curdepth = 0,
	ctr_bw_curwidth = 0
	;

unsigned 
	ctr_lowerset_intersect = 0, 
	ctr_root_intersect = 0, 
	ctr_initial_intersect = 0
	;

unsigned
	ctr_prune_dequeues = 0, 
	ctr_known_dequeues = 0
	;

unsigned
	witeration = 0, 
	piteration = 0, 
	fpcycles = 0,
	bpcycles = 0
	; //count the number of workset/pruneset elements that were processed

unsigned
	ctr_locally_pruned = 0
	;

unsigned
	update_counter = 0, 
	osz = 0, osz_max = 0,
	nsz = 0, nsz_max = 0,
	msz = 0, msz_max = 0,
	dsz = 0, dsz_max = 0,
	wsz = 0, wsz_max = 0,
	gsz = 0,
	wdsz = 0
	;


/********************************************************
Greedy backward search
********************************************************/
bool check_for_lowerset_interections(bstate_t w, list<bstate_t>& pres, vec_antichain_t& M, non_minimals_t& N, vec_antichain_t& O, pending_t& W, complement_vec_t* C, vec_antichain_t& D, bstate_t t, unsigned& piteration, unsigned& ctr_prune_dequeues, unsigned& ctr_known_dequeues, W_deferred_t& W_deferred, Net* n)
{

	auto 
		i = pres.begin(), 
		e = pres.end();

	//handle lowerset intersections
	for(; i != e ; ++i) //for all predecessors (PN <= 1, for TPN <= |w|)
	{
		bstate_t pre = *i;
		bool lowerset_intersect = false, root_intersect = false, initial_intersect = false;
		cmb_node tmp(pre->bounded_locals.begin(),pre->bounded_locals.end(),0,NULL);
		po_rel_t D_rel;
		if( !(initial_intersect = *pre <= n->init) && !(root_intersect = C->luv[pre->shared].l_nodes.find(&tmp) != C->luv[pre->shared].l_nodes.end()) && !(lowerset_intersect = ((D_rel = D.relation(pre)) == eq || D_rel == neq_le))) //check predecessors for lowerset intersection (removing this check slows down the procedure, but does not affect its correctness/termination)
			continue;

		bout << "trace to an ";
		if(initial_intersect) ++ctr_initial_intersect, bout << "initial state found";
		else if(root_intersect) ++ctr_root_intersect, bout << "already k-covered state found";
		else if(lowerset_intersect) ++ctr_lowerset_intersect, bout << "to an <k-covered state found";
		bout << endl;

		++bpcycles;

		if (!try_prune(w,M,N,O,W,*C,D,t,piteration, ctr_prune_dequeues,W_deferred))
		{
			cerr << "Backward trace to coverable state found; target " << *net.target << " is coverable" << endl;
			bw_safe = false;
		}
		else
		{
			invariant(implies(!defer, W_deferred.empty()));
			if(defer) //check could be avoided: !defer => W_deferred.empty()
			{
				foreach(bstate_t b,W_deferred)
				{
					bout << "readd " << *b << endl;
					W.push(keyprio_pair(b));

					if(b->nb->src == b)
					{
						bout << *b << " is source => reallocate upperset" << endl;
						debug_assert(b->us == nullptr);
						const_cast<BState*>(b)->us = new BState::vec_upperset_t, b->us->insert(b);
					}
				}
				W_deferred.clear();
			}
		}

		//set flag and clean up
		return true;
		foreach(bstate_t b, pres) delete b;
		pres.clear();
	}

	return false;

}

#define mark_as_new(b) (assert((b)->bl==nullptr), const_cast<BState*>(b)->bl = (BState::blocking_t*)-1)
#define is_new(b) ((b)->bl == (BState::blocking_t*)-1)
#define unmark_new(b) (assert((b)->bl==(BState::blocking_t*)-1), const_cast<BState*>(b)->bl = nullptr)

#define mark_lrel(b,rel) (assert((b)->fl==0), const_cast<BState*>(b)->fl = ((intptr_t)rel+1))
#define get_lre(b) ((po_rel_t)((intptr_t)((b)->fl)-1))
#define unmark_lrel(b) (assert(get_lre(b) == neq_le || get_lre(b) == neq_nge_nle), const_cast<BState*>(b)->fl = 0)

void clean_replace_and_mark(bstate_t src, list<bstate_t>& pres, vec_antichain_t& M, non_minimals_t& N, pending_t& W, Net& net)
{

	auto 
		i = pres.begin(), 
		e = pres.end();

	//compute set with minimal predecessor
	BState::vec_upperset_t min_predecs;
	for( ;i != e; ++i) min_predecs.insert(*i);

	//remove all locally non-minimal predecessor
	i = pres.begin(), e = pres.end();
	for( ; i != e; )
	{				
		bstate_t pre = *i;
		po_rel_t rel;
		if(!min_predecs.manages(pre) || (rel = src->us->relation(pre)) == neq_ge || rel == eq)
		{
			//predecessor is locally non-minimal => delete
			bout << *pre << " is not locally minimal => ignore it" << endl;
			auto ii = i; ++i;
			pres.erase(ii);
			delete pre;
		}
		else 
		{			
			mark_lrel(pre,rel);
			invariant(get_lre(pre) == rel);
			++i;
		}

	}

	//replace predecessor with existing ones if they already exist; mark new states
	i = pres.begin(), e = pres.end();
	for( ; i != e; )
	{				
		bstate_t pre = *i;

		non_minimals_t::const_iterator exist;
		if((exist = M.M_cref(pre->shared).find(pre)) != M.M_cref(pre->shared).end() || (exist = N.find(pre)) != N.end()) //check whether an equal state already exists
		{

			if((*exist)->nb->src == src)
			{
				bout << *pre << " already exists in the local search tree => ignore it" << endl;
				auto ii = i; ++i;
				pres.erase(ii);
				unmark_lrel(pre), delete pre;
			}
			else
			{

				bout << "old state, but on different search tree => keep" << endl; ctr_state_not_new++;
				mark_lrel(*exist,get_lre(pre));
				unmark_lrel(pre), delete pre;
				pre = *exist;
				*i = pre;

				if(net.check_target && pre->nb->sleeping) //note: *exist == *pre can occur
				{
					pre->nb->sleeping = false, W.push(keyprio_pair(pre));
					bout << "target dependant wake up: " << **exist << endl;
				}
				
				++i;
			}
		}
		else
		{
			ctr_state_new++;
			bout << "new state => keep" << endl;
			mark_as_new(pre);

			++i;
		}
	}

	return;

}

//check whether there exists a predecessor that is globally minimal; also wake up source nodes covered by any predecessor
bool check_global_minimal_and_wake_up(list<bstate_t>& pres, vec_antichain_t& M, pending_t& W, Net& net)
{

	auto 
		i = pres.begin(), 
		e = pres.end();

	for( ; i != e ; ++i)
	{

		bstate_t pre = *i;
		bout << "checking predecessor " << *pre << " (phase 1)" << endl;

		po_rel_t global_rel = M.relation(pre);
		if(global_rel == neq_nge_nle || global_rel == neq_le)
		{
			bout << *pre << " is a global minimal predecessor" << endl;
			return true; //break;
		}
		else
		{
			if(global_rel == neq_ge && net.check_target)
			{
				foreach(const bstate_t& other, M.LGE(pre,antichain_t::less_equal))
				{
					if(other->nb->sleeping)
					{
						bout << "target dependant wake up: " << *other << endl;
						other->nb->sleeping = false, W.push(keyprio_pair(other));
					}
					else
					{
						bout << "(no target dependant wake up of : " << *other << ")" << endl;
					}
				}
			}

			bout << *pre << " is not globally minimal" << endl;
		}

	}

	return false;

}

void defer_state(bstate_t w, list<bstate_t>& pres, W_deferred_t& W_deferred)
{
	//step 1: defer state
	invariant(w->nb->status == BState::processed); //status is set to processed when it is remove from the work set, which is not really correct
	w->nb->status = BState::pending;

	if(w->nb->src == w)
	{
		bout << *w << " is a source that will be deferred => delete the corresponding upperset" << endl;
		delete w->us, const_cast<BState*>(w)->us = nullptr;
	}
	invariant(w->us == nullptr);
	W_deferred.insert(w);

	//step 1: delete new predecessors
	auto 
		i = pres.begin(), 
		e = pres.end();

	for( ;i != e; ++i ) //for all predecessors (PN <= 1, for TPN <= |w|)
	{

		bstate_t pre = *i;
		unmark_lrel(pre);
		bout << "check this predecessor... " << *pre << endl;
		if(!is_new(pre))
			bout << "old state => keep" << endl;
		else
		{
			bout << "new state => delete predecessor " << *pre << endl; bout << (pre)->bl << endl;bout << (pre)->nb << endl;bout << (pre)->us << endl;switch((pre)->type){		case BState::top: bout << "top" << endl; break;case BState::def: bout << "def" << endl; break;case BState::bot: bout << "bot" << endl; break;default: bout << (pre)->type << endl; ;}
			unmark_new(pre), delete pre;
			bout << "deleted successfully" << endl;
		}
	}
	pres.clear();

	return;

}

/********************************************************
Greedy backward search
********************************************************/
void Pre2(Net* n, const unsigned k, work_pq::order_t worder, complement_vec_t* C, const vector<BState>& work_seq, bool print_cover)
{

	//statistics
	ctr_state_new = 0, ctr_state_not_new = 0, 
		ctr_locally_non_minimal = 0, ctr_locally_non_minimal__neq_ge = 0, ctr_locally_non_minimal__neq_ge__exists_max = 0, 
		ctr_locally_non_minimal__eq = 0, 
		ctr_locally_minimal = 0, ctr_locally_minimal__neq_le = 0, ctr_locally_minimal__neq_le__exists_max = 0,
		ctr_locally_minimal__neq_nge_nle = 0,
		ctr_globally_non_minimal = 0, ctr_globally_non_minimal__neq_ge = 0, ctr_globally_non_minimal__neq_ge__exists_max = 0, 
		ctr_globally_minimal = 0, ctr_globally_minimal__neq_le = 0, ctr_globally_minimal__neq_le__exists_max = 0,
		ctr_globally_minimal__neq_nge_nle = 0;

	ctr_bw_maxdepth = 0,
		ctr_bw_maxwidth = 0,
		ctr_bw_curdepth = 0,
		ctr_bw_curwidth = 0;

	ctr_lowerset_intersect = 0, 
		ctr_root_intersect = 0, 
		ctr_initial_intersect = 0;

	ctr_prune_dequeues = 0, 
		ctr_known_dequeues = 0;

	witeration = 0, 
		piteration = 0, 
		fpcycles = 0,
		bpcycles = 0; //count the number of workset/pruneset elements that were processed

	osz = 0, osz_max = 0, 
		nsz = 0, nsz_max = 0,
		msz = 0, msz_max = 0,
		dsz = 0, dsz_max = 0,
		wsz = 0, wsz_max = 0;

	ctr_locally_pruned = 0;

	work_pq				W(worder); //working set with priority "greater", "less" or "random"
	W_deferred_t		W_deferred; //state that must be readded to W up on pruning

	try
	{

		vec_antichain_t			O; //maximal antichain of non-minimal elements (elements are also in M); O <= N
		non_minimals_t			N; //non-minimal elements (not necessarily pairwise incomparable); are deleted at end of scope
		vec_antichain_t			M(true); //minimum antichain of uncovered states
		vec_antichain_t			D(true); //maximum antichain of covered states
		
		bstate_t				w = nullptr; //current work/pruning element
		bstate_nc_t				t;

		//create inital nodes
		bout << "Initializing backward data structures..." << endl;
		C->project_and_insert(net.init); //add the k-projections of state net.init to the lower set stored in C (e.g. 0|0 and 0|0,0 for initial state 0|0w and k=2)
		bout << "project_and_insert done" << endl;
		for(shared_t s = 0; s < BState::S && (print_cover || !shared_fw_done) && execution_state == RUNNING; ++s)
		{

			if(!net.core_shared(s))
				continue;

			bout << "shared state " << s << " about to be initialized... " << endl;

			foreach(const cmb_node_p n, C->luv[s].u_nodes) //traverse all minimal uncovered elements (e.g. 0|1, 1|0 and 1|1 for initial state 0|0w, S=L=2; independant of k)
			{

				if(!(print_cover || !shared_fw_done) && execution_state == RUNNING)
					break;

				BState* add = new BState(s,n->c.begin(),n->c.end(),true); //allocation of the inital state resp. node of the backward state
				add->nb->src = add, add->nb->ini = true; //the source field of a source state points to itself and its ini flag is set to true

				//TODO:  BState->bl und BState->us erst bei Gebrauch allozieren!! Unn�tige reallokationen suchen und verhinder.
				//Vielleicht auch BState->nb, aber das w�re vermutlich recht aufwendig
#ifdef EAGER_ALLOC
				add->us = new BState::vec_upperset_t, add->us->insert(add); //every source state has an associated upperset (called "local") that initially only contains this state
#endif
				add->nb->status = BState::pending, net.check_target?add->nb->sleeping = true:W.push(keyprio_pair(add));
				//M.insert(add); //every source state is a minimal element of the "global" upperset
				M.insert_incomparable(add);
			}

			bout << "shared state " << s << " initialized" << endl;
		}

		if(net.check_target && (print_cover || !shared_fw_done) && execution_state == RUNNING)
		{

			if(!M.manages(net.target))
			{
				t = new BState(net.target->shared,net.target->bounded_locals.begin(),net.target->bounded_locals.end(),true); 
				t->nb->src = t, t->nb->ini = true; //the source field of a source state points to itself and its ini flag is set to true
				t->us = new BState::vec_upperset_t, t->us->insert(t); //every source state has an associated upperset (called "local") that initially only contains this state
			}
			else
			{
				t = const_cast<bstate_nc_t>(*M.case_insert(net.target).exist_els.begin());
				delete net.target, net.target = t;
			}

			if(M.relation(t) == neq_ge)
			{
				BState::upperset_t::insert_t ins = M.case_insert(t); //get smaller elements
				N.insert(t), enqueue(ins.exist_els,O.LGE(t, vec_antichain_t::set_t::greater_equal),t), O.max_insert(t), t->nb->status = BState::blocked_pending; //the predecessor is locally, but not globally minimal; it is added to the set of non-minimal elements, its blocking edged are adjusted wrt. to existing states and its status is set to blocked_pending

				//wake up smaller states
				foreach(const bstate_t& other, ins.exist_els)
				{
					invariant(other->nb->sleeping);
					other->nb->sleeping = false, W.push(keyprio_pair(other));
					bout << "target dependant wake up: " << *other << endl;
				}
			}
			else if(M.relation(t) == neq_nge_nle)
			{
				BState::upperset_t::insert_t ins = M.case_insert(t); //get smaller elements
				enqueue(set<bstate_t>(),O.LGE(t, vec_antichain_t::set_t::greater_equal),t), W.push(keyprio_pair(t)), t->nb->status = BState::pending; //adjust blocking edges, set the status to pending and add it to the work set
			}
			else
			{
				t->nb->sleeping = false, W.push(keyprio_pair(t)); //wake it up and add it to the work set
			}
		}
		else
		{
			t = nullptr;
		}
		bout << "Initializing backward data structures... done" << endl;

		bout << "Starting backward main loop..." << endl;
		while((print_cover || !shared_fw_done) && execution_state == RUNNING)
		{

			debug_assert_foreach(bstate_t b, W_deferred, !W.contains(keyprio_pair(b)));

			std::list<std::pair<shared_t, cmb_node_p> >* m;
			while((print_cover || !shared_fw_done) && !shared_cmb_deque.lst->empty())
			{
				++fpcycles;
				m = new std::list<std::pair<shared_t, cmb_node_p> >;
				shared_cmb_deque.mtx.lock(), swap(shared_cmb_deque.lst,m), shared_cmb_deque.mtx.unlock();
				try_prune(*m,M,N,O,W,*C,D,t,piteration,ctr_prune_dequeues,ctr_known_dequeues,W_deferred);
				delete m;

				if(defer)
				{
					foreach(bstate_t b,W_deferred)
					{
						bout << "readd " << *b << endl;
						W.push(keyprio_pair(b));

						if(b->nb->src == b)
						{
							bout << *b << " is source => reallocate upperset" << endl;
							debug_assert(b->us == nullptr);
							const_cast<BState*>(b)->us = new BState::vec_upperset_t, b->us->insert(b);
						}
					}
					W_deferred.clear();
				}
			}

			if(W.empty() || (!print_cover && shared_fw_done) || !bw_safe) break;

			bout << "+++++++++++++++ Work set iteration " << witeration << " +++++++++++++++" << endl;

#if 0
			//this was just for testing whether counters were correct; not yet sure whether this is the case or not
			osz = O.size(), dsz = D.size();

			unsigned nsz_tmp = 0, msz_tmp = 0;
			//count queries
			//foreach(bstate_t n, N) if(n->us != nullptr) ++nsz;
			//for(shared_t s = 0; s < BState::S; ++s) foreach(bstate_t m,M.uv[s].M) if(m->us != nullptr) ++msz;

			foreach(bstate_t n, N) if(!n->nb->sleeping) ++nsz_tmp;
			for(shared_t s = 0; s < BState::S; ++s) foreach(bstate_t m,M.uv[s].M) if(!m->nb->sleeping) ++msz_tmp;
			swap(nsz_tmp,nsz);
			swap(msz_tmp,msz);

#else
			//if(++update_counter == 30)
			{
				osz = O.size(), nsz = N.size(), msz = M.size(), dsz = D.size(), wsz = W.size();
				osz_max = max(osz,osz_max), nsz_max = max(nsz,nsz_max), msz_max = max(msz,msz_max), dsz_max = max(dsz,dsz_max), wsz_max = max(wsz,wsz_max);
				gsz = M.graph_size();
				wdsz = W_deferred.size();
			}
			//else
			//{
				//update_counter = 0;
			//}
#endif

			if(witeration >= work_seq.size())
			{
				w = W.top().first, w->nb->status = BState::processed, W.pop(); //the work element is guaranteed to be processed

				if(writetrace)
				{
					ofstream nw((n->filename + ".trace").c_str(),ios_base::app);
					debug_assert(nw.good());
					nw << *w << endl, nw.close();
				}
			}
			else
			{
				non_minimals_t::const_iterator exist = M.M_cref(work_seq[witeration].shared).find(&work_seq[witeration]);
				debug_assert(exist != M.M_cref(work_seq[witeration].shared).end() && W.contains(keyprio_pair(*exist)));
				w = *exist, w->nb->status = BState::processed, W.erase(keyprio_pair(*exist));
			}

			if(graph_type != GTYPE_NONE) print_dot_search_graph(M,N,O,w,work_pq(),false,witeration,piteration,"_interm");

			bout << "processing " << *w << endl;
			witeration++; //increase the number of processed workset elements

			ctr_bw_maxdepth = max(ctr_bw_maxdepth, w->nb->depth), ctr_bw_maxwidth = max(ctr_bw_maxwidth, (unsigned)w->size()), ctr_bw_curdepth = w->nb->depth, ctr_bw_curwidth = w->size();

			bstate_t src = w->nb->src;

#ifndef EAGER_ALLOC
			if(src->us == nullptr)
				const_cast<BState*>(src)->us = new BState::vec_upperset_t, src->us->insert(src); //every source state has an associated upperset (called "local") that initially only contains this state
#endif

			list<bstate_t> pres; { Breached_p_t uniquepres = Pre(*w, *n); pres.insert(pres.begin(),uniquepres.begin(),uniquepres.end()); }

			auto 
				i = pres.begin(), 
				e = pres.end();

			//handle lowerset intersections
			//bool state_pruned = check_for_lowerset_interections();
			if(check_for_lowerset_interections(w,pres,M,N,O,W,C,D,t,piteration,ctr_prune_dequeues,ctr_known_dequeues,W_deferred,n))
				continue;
			
			clean_replace_and_mark(src, pres, M, N, W, net);
			if(defer && !check_global_minimal_and_wake_up(pres, M, W, net)) //check whether there exists a predecessor that is globally minimal; also wake up source nodes covered by any predecessor
			{
				defer_state(w, pres, W_deferred);
				invariant(pres.empty());
				bout << "state not processed; will be readded during the next pruning step" << endl;
			}
			else
			{
				auto 
					i = pres.begin(), 
					e = pres.end(),
					ii = e;

				bool 
					is_new;

				bstate_t 
					pre;

				for(; i != e ;) //for all predecessors (PN <= 1, for TPN <= |w|)
				{

					pre = *i, ii = i, i++, pres.erase(ii);
					is_new = is_new(pre); 
					!is_new || unmark_new(pre);

					invariant(pre->fl != 0);

					bout << "checking predecessor " << *pre << "; new state? " << is_new << endl;

					//----------- expansion -----------//
					BState::upperset_t::insert_t
						local_insert,
						global_insert;

					bout << "about to locally insert" << endl;
					bout << "src->us: " << src->us << endl;
					
					local_insert.case_type = get_lre(pre), unmark_lrel(pre);
					invariant(local_insert.case_type != neq_ge && local_insert.case_type != eq);
					if(local_insert.case_type == neq_nge_nle) src->us->insert_incomparable(pre);
					else local_insert.exist_els = src->us->insert_neq_le(pre);

					bout << "local insert done" << endl;

					switch(local_insert.case_type){
					case neq_ge:		ctr_locally_non_minimal++,	ctr_locally_non_minimal__neq_ge++, ctr_locally_non_minimal__neq_ge__exists_max = max(ctr_locally_non_minimal__neq_ge__exists_max, (unsigned)local_insert.exist_els.size()); break;
					case eq:			ctr_locally_non_minimal++,	ctr_locally_non_minimal__eq++; break;
					case neq_le:		ctr_locally_minimal++,		ctr_locally_minimal__neq_le++, ctr_locally_minimal__neq_le__exists_max = max(ctr_locally_minimal__neq_le__exists_max, (unsigned)local_insert.exist_els.size()); break;
					case neq_nge_nle:	ctr_locally_minimal++,		ctr_locally_minimal__neq_nge_nle++; break;}

					//process wpre
					if(local_insert.case_type != neq_ge && local_insert.case_type != eq) //only consider locally minimal predecessors
					{

						bout << "locally minimal" << endl;
						if(local_insert.case_type == neq_le) //check for existance of locally minimal elements that are larger and incomparable to the predecessor
						{
							bout << "local detach wrt. smaller local states: "; copy_deref_range(local_insert.exist_els.begin(), local_insert.exist_els.end(), bout); bout << endl;
							ctr_locally_pruned += local_detach(src,local_insert.exist_els,M,N,O,W,*C,D,t,W_deferred); //detach parts of the local search tree that start in (now) locally non-minimal states
						}

						if(is_new)
							const_cast<BState*>(pre)->allocate();

						w->nb->suc.insert(pre), pre->nb->pre.insert(w); //add edges between the work element and the existing predecessor

						if(is_new)
						{
							pre->nb->src = w->nb->src; //the source of the new predecessor is the source of the work element
							pre->nb->depth = 1 + w->nb->depth;

							global_insert = M.case_insert(pre);
							invariant(global_insert.case_type != eq);

							switch(global_insert.case_type){
							case neq_ge:		ctr_globally_non_minimal++,	ctr_globally_non_minimal__neq_ge++, ctr_globally_non_minimal__neq_ge__exists_max = max(ctr_globally_non_minimal__neq_ge__exists_max, (unsigned)global_insert.exist_els.size()); break;
							case neq_le:		ctr_globally_minimal++,		ctr_globally_minimal__neq_le++, ctr_globally_minimal__neq_le__exists_max = max(ctr_globally_minimal__neq_le__exists_max, (unsigned)global_insert.exist_els.size()); break;
							case neq_nge_nle:	ctr_globally_minimal++,		ctr_globally_minimal__neq_nge_nle++; break;}

							if(global_insert.case_type == neq_ge) //check whether the predecessor is >= and != existing states with a different source
							{
								bout << "globally non-minimal; blocks on "; copy_deref_range(global_insert.exist_els.begin(), global_insert.exist_els.end(), bout); bout << endl;

								N.insert(pre), enqueue(global_insert.exist_els,O.LGE(pre, vec_antichain_t::set_t::greater_equal),pre), O.max_insert(pre), pre->nb->status = BState::blocked_pending; //the predecessor is locally, but not globally minimal; it is added to the set of non-minimal elements, its blocking edged are adjusted wrt. to existing states and its status is set to blocked_pending

								if(net.check_target)
								{
									foreach(const bstate_t& other, global_insert.exist_els)
									{
										if(other->nb->sleeping)
										{
											bout << "target dependant wake up: " << *other << endl;
											other->nb->sleeping = false, W.push(keyprio_pair(other));
										}
										else
										{
											bout << "(no target dependant wake up of : " << *other << ")" << endl;
										}
									}
									bout << "existing elements processed" << endl;
								}

							}
							else
							{
								bout << "globally minimal" << endl;
								
								if(global_insert.case_type == neq_le)
								{
									bout << "blocking larger states "; copy_deref_range(global_insert.exist_els.begin(), global_insert.exist_els.end(), bout); bout << endl;

									foreach(const bstate_t& other, global_insert.exist_els)
									{
										O.max_insert(other), N.insert(other);
										if(other->nb->status == BState::pending)
										{
											invariant(iff(W.contains(keyprio_pair(other)),W_deferred.find(other) == W_deferred.end()));
											auto f = W_deferred.find(other);
											if(f == W_deferred.end())
												W.erase(keyprio_pair(other));
											else
												W_deferred.erase(f);
										}
										other->nb->status = ((other->nb->status == BState::pending)?BState::blocked_pending:BState::blocked_processed); //the larger elements are added to the set of non-minimal elements, their status is set depending on their previous state to either blocked_processed (previously processed) or blocked_pending (previously pending); in the latter case they are also removed from the working set
									}
								}

								enqueue(set<bstate_t>(),O.LGE(pre, vec_antichain_t::set_t::greater_equal),pre), W.push(keyprio_pair(pre)), pre->nb->status = BState::pending; //adjust blocking edges, set the status to pending and add it to the work set
							}
							
							bout << "global insert done" << endl;
						}
					}
					else if(is_new)
					{
						bout << "not locally minimal" << endl;
						delete pre; //note: delete "pre" if it is a new state, i.e., exactly if "wpre == pre"
					}

					bout << "predecessor processed" << endl;

				}
				bout << "state processed" << endl;
			}

		}

		if(graph_type != GTYPE_NONE) print_dot_search_graph(M,N,O,w,work_pq(),false,witeration,piteration,"_final");

		//clean-up non-minimal elements
		foreach(bstate_t b, N)
			delete b;

		debug_assert(intersection_free(D,M));

	}
	catch(std::bad_alloc)
	{
		cerr << fatal("backward exploration reached the memory limit") << endl; 
		execution_state = MEMOUT;
#ifndef WIN32
		disable_mem_limit(); //to prevent bad allocations while printing statistics
#endif
	}

	shared_bw_done = true;
	
	shared_cout_mutex.lock(), (shared_fw_done) || (shared_bw_finised_first = 1), shared_cout_mutex.unlock();
	debug_assert(implies(shared_bw_finised_first && bw_safe,W.empty()));

	if(shared_bw_finised_first) 
		finish_time = boost::posix_time::microsec_clock::local_time(), bout << "bw first" << endl;
	else 
		bout << "bw not first" << endl;

	{
		shared_cout_mutex.lock();

		statsout << endl;
		statsout << "---------------------------------" << endl;
		statsout << "Backward statistics:" << endl;
		statsout << "---------------------------------" << endl;
		statsout << "bw finished first               : " << (shared_bw_finised_first?"yes":"no") << endl;
		statsout << endl;
		statsout << "iterations                      : " << witeration + piteration << endl;
		statsout << "- work iterations               : " << witeration << endl;
		statsout << "- prune iterations              : " << piteration << endl;
		statsout << "- foward prune cycles           : " << fpcycles << endl;
		statsout << "- backward prune cycles         : " << bpcycles << endl;
		statsout << endl;
		statsout << "oracle dequeues                 : " << ctr_prune_dequeues + ctr_known_dequeues << endl;
		statsout << "- pruned                        : " << ctr_prune_dequeues << endl;
		statsout << "- known                         : " << ctr_known_dequeues << endl;
		statsout << endl;
		statsout << "new predecessors                : " << ctr_state_new << endl;
		statsout << "existing predecessors           : " << ctr_state_not_new << endl;
		statsout << endl;
		statsout << "locally_minimal                 : " << ctr_locally_minimal << endl;
		statsout << "- neq_le (max matched)          : " << ctr_locally_minimal__neq_le << " (" << ctr_locally_minimal__neq_le__exists_max << ")" << endl;
		statsout << "- neq_nge_nle                   : " << ctr_locally_minimal__neq_nge_nle << endl;
		statsout << "locally_non_minimal             : " << ctr_locally_non_minimal << endl;
		statsout << "- neq_ge (max matched)          : " << ctr_locally_non_minimal__neq_ge << " (" << ctr_locally_non_minimal__neq_ge__exists_max << ")" << endl;
		statsout << "- eq                            : " << ctr_locally_non_minimal__eq << endl;
		statsout << endl;
		statsout << "globally_minimal                : " << ctr_globally_minimal << endl;
		statsout << "- neq_le (max matched)          : " << ctr_globally_minimal__neq_le << " (" << ctr_globally_minimal__neq_le__exists_max << ")" << endl;
		statsout << "- neq_nge_nle                   : " << ctr_globally_minimal__neq_nge_nle << endl;
		statsout << "globally_non_minimal            : " << ctr_globally_non_minimal << endl;
		statsout << "- neq_ge (max matched)          : " << ctr_globally_non_minimal__neq_ge << " (" << ctr_globally_non_minimal__neq_ge__exists_max << ")" << endl;
		statsout << endl;
		statsout << "lowerset intersections          : " << ctr_initial_intersect + ctr_root_intersect + ctr_lowerset_intersect << endl;
		statsout << "- initial_intersect             : " << ctr_initial_intersect << endl;
		statsout << "- root_intersect                : " << ctr_root_intersect << endl;
		statsout << "- lowerset_intersect            : " << ctr_lowerset_intersect << endl;
		statsout << endl;
		statsout << "locally_pruned                  : " << ctr_locally_pruned << endl;
		statsout << "---------------------------------" << endl;
		statsout << "max. backward depth checked     : " << ctr_bw_maxdepth << endl;
		statsout << "max. backward width checked     : " << ctr_bw_maxwidth << endl;
		statsout << "---------------------------------" << endl;
		statsout << "osz_max                         : " << osz_max << endl;
		statsout << "nsz_max                         : " << nsz_max << endl;
		statsout << "msz_max                         : " << msz_max << endl;
		statsout << "dsz_max                         : " << dsz_max << endl;
		statsout << "wsz_max                         : " << wsz_max << endl;
		statsout << "---------------------------------" << endl;

		shared_cout_mutex.unlock();
	}

}

#endif
