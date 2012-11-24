/******************************************************************************
  Synopsis		[Modified Karp-Miller procedure with transfer support.]

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

#ifndef FW_H
#define FW_H

#include "options_str.h"
#include <stack>

const string accel_edge_style = "style=dotted,arrowhead=tee,arrowsize=\"0.5\"";
void print_dot_search_graph(const Oreached_t& Q)
{

	fw_log << "writing coverability tree..." << "\n", fw_log.flush();
	
	string out_fn = net.filename + ".fw-tree.dot";

	set<ostate_t> seen;
	stack<ostate_t> S;

	ofstream out(out_fn.c_str());
	if(!out.good()) throw logic_error((string)"cannot write to " + out_fn);
	
	out << "digraph BDD {" << "\n";
	foreach(ostate_t s, Q)
	{
		if(s->prede == nullptr)
			out << "{rank=source; " << '"' << s->str_id() << '"' << '}' << "\n";

		out << '"' << s->str_id() << '"' << ' ' << '[' << "lblstyle=" << '"' << "kmnode" << '"' << ",texlbl=" << '"' << s->str_latex() << '"' << ']' << ';' << "\n";

		if(s->prede != nullptr)
			if(s->tsucc)
				switch(graph_type){
				case GTYPE_DOT: out << '"' << s->prede->str_id() << '"' << " -> " << '"' << s->str_id() << '"' << "[style=dashed];" << "\n"; break;
				case GTYPE_TIKZ: out << '"' << s->prede->str_id() << '"' << " -> " << '"' << s->str_id() << '"' << "[style=transfer_trans];" << "\n"; break;}
			else
				switch(graph_type){
				case GTYPE_DOT: out << '"' << s->prede->str_id() << '"' << " -> " << '"' << s->str_id() << '"' << ';' << "\n"; break;
				case GTYPE_TIKZ: out << '"' << s->prede->str_id() << '"' << " -> " << '"' << s->str_id() << '"' << "[style=thread_trans];" << "\n"; break;}

		if(s->accel != nullptr)
			switch(graph_type){
				case GTYPE_DOT: out << '"' << s->str_id() << '"' << " -> " << '"' << s->accel->str_id() << '"' << " [" << accel_edge_style << "];" << "\n"; break;
				case GTYPE_TIKZ: out << '"' << s->str_id() << '"' << " -> " << '"' << s->accel->str_id() << '"' << "[style=accel_edge];" << "\n"; break;}
	}

	out << "}" << "\n";
	out.close();

}


enum core_state_t{
	SHARED,
	LOCAL,
	FULL };

//bool is_core_state(ostate_t s, Net& n, core_state_t t = FULL)
//{
//	bool ret = true;
//
//	if(t!=LOCAL) 
//		ret &= core_shared[s->shared];
//
//#ifndef NO_LOCAL_POR
//	if(t!=SHARED){
//		for(const local_t& l : s->bounded_locals)
//			ret &= n.core_local(l);
//		for(const local_t& l : s->unbounded_locals)
//			ret &= n.core_local(l);
//	}
//#endif
//
//	return ret;
//}

typedef vector<pair<Net::adj_t::const_iterator, trans_type> > tt_list_t;

Oreached_t Post(ostate_t ag, Net& n, lowerset_vec& D, shared_cmb_deque_t& shared_cmb_deque)
{ 
	
	Oreached_t succs;

	/*
	//TODO: remove
	const_cast<OState*>(ag)->unbounded_locals.clear();
	//const_cast<OState*>(ag)->unbounded_locals.insert(0);
	//const_cast<OState*>(ag)->bounded_locals.insert(2);
	const_cast<OState*>(ag)->bounded_locals.insert(0);
	const_cast<OState*>(ag)->bounded_locals.insert(0);
	const_cast<OState*>(ag)->bounded_locals.insert(1);
	fw_log << "computing successor of " << *ag << "\n"; //TODO: remove
	*/
	
	stack<OState*> work;

	work.push(const_cast<OState*>(ag));

	fw_log << "computing successors" << "\n";
	while(!work.empty())
	{
		OState* g = work.top(); work.pop();
		bool g_in_use = g == ag;

		invariant(iff(g == ag,core_shared[g->shared]));

		//collect all relevant transitions
		tt_list_t tt_list;
		
		Net::adj_t::const_iterator tc;
		
		for(auto l = g->bounded_locals.begin(), le = g->bounded_locals.end();l != le;)
		{
			if((tc = n.adjacency_list.find(Thread_State(g->shared, *l))) != n.adjacency_list.end()) 
				tt_list.push_back(make_pair(tc, thread_transition));

			local_t last = *l;
			while(++l != le && last == *l); //skip same locals
		}
		for(auto l = g->unbounded_locals.begin(), le = g->unbounded_locals.end(); l != le; ++l)
		{
			if((tc = n.adjacency_list.find(Thread_State(g->shared, *l))) != n.adjacency_list.end()) 
				tt_list.push_back(make_pair(tc, thread_transition));
		}

		//compute successors
		auto tt = tt_list.begin(), 
			ttnex = tt_list.begin(), 
			tte = tt_list.end();

		for(; tt != tte; ++tt){
			const trans_type& ty = tt->second;
			const Thread_State& u = tt->first->first; 
			invariant(g->shared == u.shared);

			auto tsb = tt->first->second.begin(),
				tsbnex = tt->first->second.begin(), //only for the type
				tse = tt->first->second.end();

			for(;tsb != tse; ++tsb)
			{
				const Thread_State& v = tsb->first;
				const transfers_t& p = tsb->second;

				const bool	bounded_in_u = g->bounded_locals.find(u.local) != g->bounded_locals.end();
				const bool	unbounded_in_u = g->unbounded_locals.find(u.local) != g->unbounded_locals.end(); 
				const bool	bounded_in_v = g->bounded_locals.find(v.local) != g->bounded_locals.end();
				bool		unbounded_in_v = g->unbounded_locals.find(v.local) != g->unbounded_locals.end();
				const bool	thread_in_u = bounded_in_u || unbounded_in_u;
				const bool	horiz_trans = (u.shared == v.shared);
				const bool	diff_locals = (u.local != v.local);

				invariant(!bounded_in_u || !unbounded_in_u);

				if((!thread_in_u && (ty == thread_transition || horiz_trans)))
				{
					fw_log << "Ignore: No/no incomparable successors" << "\n";
					continue;
				}

				OState* succ;
				if(!g_in_use && ++(ttnex = tt) == tte && ++(tsbnex = tsb) == tse)
				{
					fw_log << "Reuse existing state " << *g << "\n";
					succ = g, g_in_use = true;
				}
				else
					succ = new OState(g->shared,g->bounded_locals.begin(),g->bounded_locals.end(),g->unbounded_locals.begin(),g->unbounded_locals.end()); //note: even in this case the there might not exist another successor (the continue above could happen)

				succ->tsucc = (g != ag && g->tsucc) || ty == transfer_transition;

				if(!horiz_trans) 
					succ->shared = v.shared;

				if(!p.empty() || (diff_locals && thread_in_u && !(ty == thread_transition && unbounded_in_u && unbounded_in_v)))
				{
					//remove one bounded occurence of u.local
					if(ty == thread_transition && bounded_in_u) succ->bounded_locals.erase(succ->bounded_locals.find(u.local)); 

					if(!p.empty())
					{
						//distribute omegas (this yields the maximal obtainable state)
						set<local_t> distibute;
						for(const local_t &passive_source : succ->unbounded_locals)
						{
							auto x = p.find(passive_source);
							if(x == p.end()) continue;
							unbounded_in_v |= x->second.find(v.local) != x->second.end();
							distibute.insert(x->second.begin(),x->second.end());
						}
						succ->unbounded_locals.insert(distibute.begin(),distibute.end());

						//create, for all passive updates combinations c_i in bounded local states, a state s_i = s+c_i (check for duplicates, and ignoring those entering an "omega"-value)
						typedef pair<bounded_t,bounded_t> local_tp; //unprocessed and processed local states
						typedef set<local_tp> comb_t;
						if(!succ->bounded_locals.empty())
						{
							comb_t combs_interm; 
							stack<comb_t::const_iterator> cwork;
							cwork.push(combs_interm.insert(make_pair(succ->bounded_locals,bounded_t())).first); //inserte pair (locals,{})

							bool first = true;
							while(!cwork.empty())
							{
								comb_t::const_iterator w = cwork.top(); cwork.pop();
								invariant(w->first.size()); //work shall only contain states with unprocessed local states

								//remove one local and create a new pair for all successor combinations
								local_t passive_source = *w->first.begin();
								auto x = p.find(passive_source); //transfer destinations

								local_t passive_target;
								set<local_t>::const_iterator pi;

								if(x == p.end()) passive_target = passive_source;
								else pi = x->second.begin(), passive_target = *pi;

								do
								{
									local_tp ne(*w);
									if(succ->unbounded_locals.find(passive_target) == succ->unbounded_locals.end()) //ignore target locals that are unbounded in the target state
										ne.second.insert(passive_target);
									invariant(*ne.first.begin() == passive_source);
									ne.first.erase(ne.first.find(passive_source));

									if(ne.first.empty())
									{

										//create new states for all but the first one; the first one is added in the remainder of this function
										if(first)
											//do not readd active thread (done below)
												succ->bounded_locals = ne.second, first = false;
										else
										{
											OState* succp = new OState(succ->shared,ne.second.begin(),ne.second.end(),succ->unbounded_locals.begin(),succ->unbounded_locals.end());
											//readd active thread
											if(!unbounded_in_v) 
												if(unbounded_in_u && horiz_trans)
													succp->unbounded_locals.insert(v.local);
												else
													succp->bounded_locals.insert(v.local);

											invariant(succp->consistent());

											if(core_shared[succp->shared])
											{
												fw_log << "Found passive core successor: " << *succp << "\n";
												succs.insert(succp); //add to return set
											}
											else
											{
												fw_log << "Found passive intermediate successor: " << *succp << "\n";
												work.push(succp); //add to work list
											}			
										}
									}
									else
									{
										auto a = combs_interm.insert(ne);
										if(a.second) cwork.push(a.first);
									}
								}
								while(x != p.end() && ++pi != x->second.end() && (passive_target = *pi,1));
							}
						}
					}

					//add one/unbounded v.local's if an unbounded number do not yet exist
					if(!unbounded_in_v) 
						if(unbounded_in_u && horiz_trans)
							succ->unbounded_locals.insert(v.local);
						else
							succ->bounded_locals.insert(v.local);
				}
				invariant(succ->consistent());
				
				if(core_shared[succ->shared])
				{
					fw_log << "Found core successor: " << *succ << "\n";
					succs.insert(succ); //add to return set
				}
				else
				{
					fw_log << "Found intermediate successor: " << *succ << "\n";
					work.push(succ); //add to work list
				}			
			}
		}

		if(!g_in_use)
			delete g;
	}

	static bool resolved_once = false;
	invariant_foreach(ostate_t s, succs, implies(resolve_omegas && resolved_once,s->unbounded_locals.empty()));
	if(resolve_omegas && !resolved_once)
	{
		fw_log << "resolving successors" << "\n";

		Oreached_t succs_resolved;
		
		foreach(ostate_t p_c, succs)
		{
			if(p_c->unbounded_locals.empty()) succs_resolved.insert(p_c);
			else
			{
				Oreached_t resolved_state(p_c->resolve_omegas(max_fw_width)); //all states with width max_fw_width are blocked anyway
				succs_resolved.insert(resolved_state.begin(),resolved_state.end());

				fw_log << "resolved " << *p_c << " to "; copy_deref_range(succs_resolved.begin(), succs_resolved.end(), fw_log); fw_log << "\n";

				fw_state_blocked = true;
				delete p_c;
			}
		}
		
		succs_resolved.swap(succs);
		resolved_once = true;
	}

	return succs;

}

template<class T>
void remove_all(multiset<T>& ms, const T& e)
{
	ms.erase(ms.lower_bound(e),ms.upper_bound(e));
}

bool accel(ostate_t q, OState* c)
{
	if(!(*q<=*c && !(*q == *c)) )
		return 0;

	//find bounded elements that occure more often in c than in q
	multiset<local_t> more_locals = c->bounded_locals;
	for(auto l = q->bounded_locals.begin(), e = q->bounded_locals.end(); l != e; ++l)
		if(c->unbounded_locals.find(*l) == c->unbounded_locals.end())
			more_locals.erase(more_locals.find(*l));

	//remove all remaining locals from the bounded locals of c and add them to the unbounded part
	for(auto l = more_locals.begin(), e = more_locals.end(); l != e; ++l)
		remove_all(c->bounded_locals, *l), c->unbounded_locals.insert(*l); //unbounded is a set, so multiple inserts are ok

	return 1;
}

void print_hash_stats(const Oreached_t& Q)
{
	Oreached_t::size_type max_bucket_size = 0;
	map<unsigned, unsigned> bucket_size_counters;
	for(int i=0;i<10;i++) bucket_size_counters[i]=0;

	unsigned Q_size = 0;
	for(Oreached_t::size_type i = 0; i < Q.bucket_count(); ++i)
		max_bucket_size = max(max_bucket_size, Q.bucket_size(i)),
		bucket_size_counters[min((unsigned)Q.bucket_size(i),9U)]++,
		Q_size += Q.bucket_size(i);

	fw_stats << "Maximum bucket size             : " << max_bucket_size << "\n";
	fw_stats << "Hash load factor                : " << Q.load_factor() << "\n";
	fw_stats << "Max collisions                  : " << max_bucket_size << "\n";

	for(map<unsigned, unsigned>::const_iterator i = bucket_size_counters.begin(), ie = bucket_size_counters.end(); i != ie; ++i)
		if(i->first < 9) fw_stats << "Buckets of size " << boost::lexical_cast<string>(i->first) << "               : " << i->second << "\n";
		else fw_stats << "Buckets of size >" << boost::lexical_cast<string>(i->first) << "              : " << i->second << "\n";
}

unsigned 
	f_its = 0
	;

unsigned 
	max_depth_checked = 0, 
	max_accel_depth = 0,
	accelerations = 0
	;

unsigned
	ctr_fw_maxdepth = 0,
	ctr_fw_maxwidth = 0
	;

unsigned fw_qsz = 0,
	fw_wsz = 0
	;

unordered_priority_set<ostate_t>::keyprio_type keyprio_pair(ostate_t s)
{ 
	//invariant(fw_weight == depth || fw_weight == width);

	switch(fw_weight)
	{
	case order_width: 
		return make_pair(s,s->size());
	case order_depth: 
		return make_pair(s,s->depth);
	default:	
		return make_pair(s,0);
	}
}

void do_fw_bfs(Net* n, unsigned ab, lowerset_vec* D, shared_cmb_deque_t* shared_cmb_deque, bool forward_projections, unordered_priority_set<ostate_t>::order_t forder)
{

	const bool plain_km = false;
	fw_start_time = boost::posix_time::microsec_clock::local_time();

	Oreached_t				Q;
	OState_priority_queue_t W(forder);
	
	fw_state_blocked = false;
	try
	{

		ostate_t init_p;
		init_p = new OState(n->init), Q.insert(init_p), W.push(keyprio_pair(init_p)), D->project_and_insert(*init_p,*shared_cmb_deque,false); //create the initial state and insert it into the reached, work and projection set 
		invariant(init_p->prede == nullptr && init_p->accel == nullptr && init_p->tsucc == false);
		
		ostate_t cur, walker;
		unsigned depth;

		while(execution_state == RUNNING && fw_safe && (!shared_bw_done || !bw_safe) && W.try_get(cur)) //retrieve the next working element
		{

			++f_its;
			
			fw_log << "Exploring: " << *cur << " (depth: " << cur->depth << ")" << "\n";
			ctr_fw_maxdepth = max(ctr_fw_maxdepth,cur->depth), ctr_fw_maxwidth = max(ctr_fw_maxwidth,(unsigned)cur->size());

			fw_qsz = Q.size();
			fw_wsz = W.size();

			if(fw_threshold != OPT_STR_FW_THRESHOLD_DEFVAL && f_its%1000==0) //just do this from time to time
			{
				auto diff = (boost::posix_time::microsec_clock::local_time() - last_new_prj_found);
				if((unsigned)(diff.hours() * 3600 + 60 * diff.minutes() + diff.seconds()) > fw_threshold)
					threshold_reached = true;
			}

			//project

			unsigned c = 0;
			//if(is_core_state(cur,net,FULL))
				c = D->project_and_insert(*cur, *shared_cmb_deque, forward_projections); //project if requested
			
			if(c)
				last_new_prj_found = boost::posix_time::microsec_clock::local_time(), 
				fw_prj_found += c, 
				fw_last_prj_width = cur->size(),
				fw_last_prj_states = Q.size(),
				fw_log << "found " << c << " projections" << "\n"
				;
			
			static bool first = true;
			if(first && threshold_reached)
				fw_log << "threshold reached" << "\n", first = false;

			foreach(ostate_t p_c, Post(cur, *n, *D, *shared_cmb_deque)) //allocates objects in "successors"
			{
				
				if((threshold_reached) || (max_fw_width != OPT_STR_FW_WIDTH_DEFVAL && p_c->size() > max_fw_width)) //TEST
				{
					fw_log << *p_c << " blocked" << "\n";
					fw_state_blocked = true;
					delete p_c;
					continue;
				}

				OState* p = const_cast<OState*>(p_c); invariant(p->accel == nullptr);

				fw_log << "checking successor " << *p << "\n";

				if(net.target.type != BState::invalid && n->target <= *p)
				{
					fw_safe = false;
					fw_log << "Forward trace to target covering state found" << "\n";

					//print trace
					ostate_t walker = cur;
					
					OState tmp(*p); if(net.S != net.S_input) tmp.unbounded_locals.erase(net.S_input);
					main_inf << "counterexample" << "\n";
					main_inf << tmp << "\n";

					if(walker->shared < net.S_input){
						OState tmp(*walker); if(net.S != net.S_input) tmp.unbounded_locals.erase(net.S_input);
						main_inf << tmp << "\n";
					}
					while(walker->prede != nullptr)
					{
						if(walker->prede->shared < net.S_input) 
						{
							OState tmp(*walker->prede); if(net.S != net.S_input) tmp.unbounded_locals.erase(net.S_input);
							main_inf << tmp << "\n";
						}
						walker = walker->prede;
					}
					main_inf.flush();
					break;
				}
				
				if(!p->tsucc || plain_km)
				{ //try to accelerate
					
					walker = cur, depth = 0;
					while(walker != init_p && (!walker->tsucc || plain_km) && depth++ < ab)
					{
						if(accel(walker, p)) //returns true if p was accelerated wrt. walker
						{
							++accelerations, max_accel_depth = max(max_accel_depth,depth), const_cast<OState*>(p)->accel = walker;
							//fw_log << "accelerated to " << *p << " based on predecessor " << *walker << "\n";
							break;
						}

						walker = walker->prede;
					}
					
					max_depth_checked = max(max_depth_checked, depth);
				}

				p->prede = cur, p->depth = 1 + cur->depth; 
				if(Q.insert(p).second)
					W.push(keyprio_pair(p)); // fw_log << "added" << "\n";
				else
					delete p;

			}

		}

		threshold_reached = true;

	}
	catch(std::bad_alloc)
	{
		cerr << fatal("forward exploration reached the memory limit") << "\n"; 
		execution_state = MEMOUT;
#ifndef WIN32
		disable_mem_limit(); //to prevent bad allocations while printing statistics
#endif
	}

	fw_log << "fw while exit" << "\n", fw_log.flush();

	while(net.target.type != BState::invalid && !shared_bw_done && print_cover)
	{
		fw_log << "waiting for bw\n";
		boost::this_thread::sleep(milliseconds(100));
	}

	if(!fw_safe || !fw_state_blocked){
		shared_fw_done = true;
	
		fwbw_mutex.lock(), (shared_bw_done) || (shared_fw_finised_first = 1), fwbw_mutex.unlock();
	
		invariant(implies(shared_fw_finised_first,!fw_safe || W.empty()));
	}
	
	if(shared_fw_finised_first) 
		finish_time = boost::posix_time::microsec_clock::local_time(), fw_log << "fw first" << "\n", fw_log.flush();
	else 
		fw_log << "fw not first" << "\n", fw_log.flush();
	
	if(tree_type != GTYPE_NONE) print_dot_search_graph(Q);

	{
		fw_stats << "\n";
		fw_stats << "---------------------------------" << "\n";
		fw_stats << "Forward statistics:" << "\n";
		fw_stats << "---------------------------------" << "\n";
		fw_stats << "fw finished first               : " << (shared_fw_finised_first?"yes":"no") << "\n";
		fw_stats << "fw execution state              : "; 
		switch(execution_state){
		case TIMEOUT: fw_stats << "TIMEOUT" << "\n"; break;
		case MEMOUT: fw_stats << "MEMOUT" << "\n"; break;
		case RUNNING: fw_stats << "RUNNING" << "\n"; break;
		case INTERRUPTED: fw_stats << "INTERRUPTED" << "\n"; break;
		}
		fw_stats << "\n";
		fw_stats << "forward-reachable global states : " << Q.size() << "\n";
		fw_stats << "total forward time              : " << (((float)(boost::posix_time::microsec_clock::local_time() - fw_start_time).total_microseconds())/1000000) << "\n";
		fw_stats << "\n";
		fw_stats << "projections found               : " << D->size() << "\n";
		fw_stats << "time to find projections        : " << (((float)(last_new_prj_found - fw_start_time).total_microseconds())/1000000) << "\n";
		fw_stats << "width of last projections       : " << fw_last_prj_width << "\n";
		fw_stats << "states at last projections      : " << fw_last_prj_states << "\n";
		fw_stats << "\n";
		fw_stats << "accelerations                   : " << accelerations << "\n";
		fw_stats << "max acceleration depth checked  : " << max_depth_checked << "\n";
		fw_stats << "max acceleration depth          : " << max_accel_depth << "\n";
		if(print_hash_info) fw_stats << "\n", print_hash_stats(Q);
		fw_stats << "---------------------------------" << "\n";
		fw_stats << "max. forward depth checked      : " << ctr_fw_maxdepth << "\n";
		fw_stats << "max. forward width checked      : " << ctr_fw_maxwidth << "\n";
		fw_stats << "---------------------------------" << "\n";
		fw_stats << "state blocked                   : " << (fw_state_blocked?"yes":"no") << "\n";
		fw_stats << "---------------------------------" << "\n";
		fw_stats.flush();
	}

	foreach(ostate_t p, Q) //contains init_p
		delete p;
	
	return;
}
#endif
