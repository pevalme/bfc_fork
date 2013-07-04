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

	fw_log << "writing coverability tree..." << endl, fw_log.flush();
	
	string out_fn = net.filename + ".fw-tree.dot";

	set<ostate_t> seen;
	stack<ostate_t> S;

	ofstream out(out_fn.c_str());
	if(!out.good()) throw logic_error((string)"cannot write to " + out_fn);
	
	out << "digraph BDD {" << endl;
	foreach(ostate_t s, Q)
	{

		bool maximal_state = true;
		for(auto& b : Q)
		{
			if(s != b && *s <= *b)
				maximal_state = false, fw_log << "state " << *s << " is not maximal; it's covered by " << *b << endl;
		}

		if(s->prede == nullptr) out << "{rank=source; " << '"' << s->id_str() << '"' << '}' << endl;
		
		switch(graph_type){
		case GTYPE_DOT: s->mindot(out,false,maximal_state?"plaintext":"ribosite"), out << endl; break;
		case GTYPE_TIKZ: out << '"' << s->id_str() << '"' << ' ' << '[' << "lblstyle=" << '"' << "fwnode" << (maximal_state?",maxstate":",nonmaxstate") << ",align=center" << '"' << ",texlbl=" << '"' << s->str_latex() << '"' << ']' << ';' << endl; }

		if(s->prede != nullptr)
		{
			string prename = s->prede->id_str();
			if(s->tsucc)
				switch(tree_type){
				case GTYPE_DOT: out << '"' << prename << '"' << " -> " << '"' << s->id_str() << '"' << "[style=dashed];" << endl; break;
				case GTYPE_TIKZ: out << '"' << prename << '"' << " -> " << '"' << s->id_str() << '"' << "[style=transfer_trans];" << endl; break;}
			else
				switch(tree_type){
				case GTYPE_DOT: out << '"' << prename << '"' << " -> " << '"' << s->id_str() << '"' << ';' << endl; break;
				case GTYPE_TIKZ: out << '"' << prename << '"' << " -> " << '"' << s->id_str() << '"' << "[style=thread_trans];" << endl; break;}
		}

		if(s->accel != nullptr)
			switch(tree_type){
				case GTYPE_DOT: out << '"' << s->id_str() << '"' << " -> " << '"' << s->accel->id_str() << '"' << " [" << accel_edge_style << "];" << endl; break;
				case GTYPE_TIKZ: out << '"' << s->id_str() << '"' << " -> " << '"' << s->accel->id_str() << '"' << "[style=accel_edge];" << endl; break;}
	}

	out << "}" << endl;
	out.close();

	string cmd = "dot -T pdf " + out_fn + " -o " + out_fn + ".pdf";
	(void)system(cmd.c_str());
	fw_log << cmd << endl;

}

typedef vector<Net::adj_t::const_iterator> tt_list_t;

#define FINALIZE_TRANS(succp) \
											auto x = p.find(u.local);\
											const bool others_in_u_were_move = x != p.end() && x->second.find(u.local) == x->second.end(); /*vastly overapproximates without this condition: see regression test accel_fault_vs for an example*/ \
											auto y = p.find(v.local); \
											const bool v_local_is_force_to_move_in_the_transition = y != p.end() && y->second.find(v.local) == y->second.end(); /*add one/unbounded v.local's if an unbounded number do not yet exist*/ \
											if(!unbounded_in_v) \
												if(unbounded_in_u && horiz_trans && !others_in_u_were_move && !v_local_is_force_to_move_in_the_transition) \
												{ \
													succp->unbounded_locals.insert(v.local); \
													if(bounded_in_v) \
														succp->bounded_locals.erase(v.local); /*0 0 -> 0 1 in state 0|1/0 -> 0|/0,1*/ \
												} \
												else \
													succp->bounded_locals.insert(v.local);

Oreached_t Post(ostate_t ag, Net& n, lowerset_vec& D, shared_cmb_deque_t& shared_cmb_deque)
{ 
	
	Oreached_t succs;

	stack<OState*> work;

	work.push(const_cast<OState*>(ag));

	fw_log << "computing successors" << endl;
	while(!work.empty())
	{
		OState* g = work.top(); work.pop();
		bool g_in_use = g == ag;

		invariant(iff(g == ag,n.core_shared[g->shared]));

		//collect all relevant transitions
		tt_list_t tt_list;
		
		Net::adj_t::const_iterator tc;
		
		for(auto l = g->bounded_locals.begin(), le = g->bounded_locals.end();l != le;)
		{
			if((tc = n.adjacency_list.find(Thread_State(g->shared, *l))) != n.adjacency_list.end()) 
				tt_list.push_back(tc);

			local_t last = *l;
			while(++l != le && last == *l); //skip same locals
		}
		for(auto l = g->unbounded_locals.begin(), le = g->unbounded_locals.end(); l != le; ++l)
		{
			if((tc = n.adjacency_list.find(Thread_State(g->shared, *l))) != n.adjacency_list.end()) 
				tt_list.push_back(tc);
		}

		//compute successors
		auto tt = tt_list.begin(), 
			ttnex = tt_list.begin(), 
			tte = tt_list.end();

		for(; tt != tte; ++tt){
			const Thread_State& u = (*tt)->first; 
			invariant(g->shared == u.shared);

			auto tsb = (*tt)->second.begin(),
				tsbnex = (*tt)->second.begin(), //only for the type
				tse = (*tt)->second.end();

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

				if(!thread_in_u)
				{
					fw_log << "Ignore: No/no incomparable successors" << endl;
					continue;
				}

				OState* succ;
				if(!g_in_use && ++(ttnex = tt) == tte && ++(tsbnex = tsb) == tse)
				{
					fw_log << "Reuse existing state " << *g << endl;
					succ = g, g_in_use = true;
				}
				else
					succ = new OState(g->shared,g->bounded_locals.begin(),g->bounded_locals.end(),g->unbounded_locals.begin(),g->unbounded_locals.end()); //note: even in this case the there might not exist another successor (the continue above could happen)

				succ->tsucc = (g != ag && g->tsucc) || !p.empty();

				if(!horiz_trans) 
					succ->shared = v.shared;

				if(!p.empty() || (diff_locals && thread_in_u && !(unbounded_in_u && unbounded_in_v)))
				{
					//remove one bounded occurence of u.local
					if(bounded_in_u) 
						succ->bounded_locals.erase(succ->bounded_locals.find(u.local)); 

					if(!p.empty())
					{
						//distribute omegas (this yields the maximal obtainable state)

						/*
						#init   0/0,1
						#target 1|1
						2 3
						0 0 -> 1 0 1 ~> 2
						*/

						set<local_t> distibute;
						for(auto i = succ->unbounded_locals.begin(); i != succ->unbounded_locals.end(); )
						{
							const local_t &passive_source = *i;
							auto x = p.find(passive_source);
							if(x == p.end()) { ++i; continue; }  //passive_source is forced to stay in its local state
							
							const bool forced_to_move = x->second.find(passive_source) == x->second.end();
							unbounded_in_v |= x->second.find(v.local) != x->second.end();
							distibute.insert(x->second.begin(),x->second.end());
							
							if(forced_to_move) i = succ->unbounded_locals.erase(i);
							else ++i;
						}
						succ->unbounded_locals.insert(distibute.begin(),distibute.end());
						
#if 0
						set<local_t> distibute, erase;
						for(const local_t &passive_source : succ->unbounded_locals)
						{
							auto x = p.find(passive_source);
							if(x == p.end()) continue;
							unbounded_in_v |= x->second.find(v.local) != x->second.end();
							distibute.insert(x->second.begin(),x->second.end());
						}
						succ->unbounded_locals.insert(distibute.begin(),distibute.end());
#endif

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

								invariant(implies(x != p.end(),!x->second.empty()));

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

											FINALIZE_TRANS(succp);

											invariant(succp->consistent());

											if(n.core_shared[succp->shared])
											{
												fw_log << "Found passive core successor: " << *succp << endl;
												succs.insert(succp); //add to return set
											}
											else
											{
												fw_log << "Found passive intermediate successor: " << *succp << endl;
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
					FINALIZE_TRANS(succ);
				}
				invariant(succ->consistent());
				
				if(n.core_shared[succ->shared])
				{
					fw_log << "Found core successor: " << *succ << endl;
					succs.insert(succ); //add to return set
				}
				else
				{
					fw_log << "Found intermediate successor: " << *succ << endl;
					work.push(succ); //add to work list
				}
				static set<Transition>ACT;
				if(ACT.insert(Transition(u,v,p)).second)
					fw_log << "new active transition: " << Transition(u,v,p) << endl;
			}
		}

		if(!g_in_use)
			delete g;
	}

	static bool resolved_once = false;
	invariant_foreach(ostate_t s, succs, implies(resolve_omegas && resolved_once,s->unbounded_locals.empty()));
	if(resolve_omegas && !resolved_once)
	{
		fw_log << "resolving successors" << endl;

		Oreached_t succs_resolved;
		
		foreach(ostate_t p_c, succs)
		{
			if(p_c->unbounded_locals.empty()) succs_resolved.insert(p_c);
			else
			{
				Oreached_t resolved_state(p_c->resolve_omegas(max_fw_width)); //all states with width max_fw_width are blocked anyway
				succs_resolved.insert(resolved_state.begin(),resolved_state.end());

				fw_log << "resolved " << *p_c << " to "; copy_deref_range(succs_resolved.begin(), succs_resolved.end(), fw_log); fw_log << endl;

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

	const bool plain_km = false; //always try to accelerate (may overapproximate in the presence of broadcast side-effects)
	fw_start_time = boost::posix_time::microsec_clock::local_time();

	Oreached_t				Q;
	OState_priority_queue_t W(forder);
	
	if(net.target <= n->init)
		fw_safe = false, fw_log << "Target is covered by the initial state" << endl;

	fw_state_blocked = false;
	try
	{

		ostate_t init_p;
		init_p = new OState(n->init), Q.insert(init_p), W.push(keyprio_pair(init_p)); //create the initial state and insert it into the reached, work and projection set 
		//D->project_and_insert(*init_p,*shared_cmb_deque,true)
		invariant(init_p->prede == nullptr && init_p->accel == nullptr && init_p->tsucc == false);
		
		ostate_t cur, walker;
		unsigned depth;

		while(exe_state == RUNNING && fw_safe && (!shared_bw_done || !bw_safe) && W.try_get(cur)) //retrieve the next working element
		{

			++f_its;
			
			fw_log << "Exploring: " << *cur << " (depth: " << cur->depth << ")" << endl;
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
				fw_log << "found " << c << " projections" << endl
				;
			
			static bool first = true;
			if(first && threshold_reached)
				fw_log << "threshold reached" << endl, first = false;

			foreach(ostate_t p_c, Post(cur, *n, *D, *shared_cmb_deque)) //allocates objects in "successors"
			{
				
				if((threshold_reached) || (max_fw_width != OPT_STR_FW_WIDTH_DEFVAL && p_c->size() > max_fw_width)) //TEST
				{
					fw_log << *p_c << " blocked" << endl;
					fw_state_blocked = true;
					delete p_c;
					continue;
				}

				OState* p = const_cast<OState*>(p_c); invariant(p->accel == nullptr);

				fw_log << "checking successor " << *p << endl;

				if(net.target.type != BState::invalid && n->target <= *p)
				{
					fw_safe = false;
					fw_log << "Forward trace to target covering state found" << endl;

					//print trace
					ostate_t walker = cur;
					
					OState tmp(*p); if(net.S != net.S_input) tmp.unbounded_locals.erase(net.S_input);
					main_inf << "counterexample" << endl;
					main_inf << tmp << endl;

					if(walker->shared < net.S_input){
						OState tmp(*walker); if(net.S != net.S_input) tmp.unbounded_locals.erase(net.S_input);
						main_inf << tmp << endl;
					}
					while(walker->prede != nullptr)
					{
						if(walker->prede->shared < net.S_input) 
						{
							OState tmp(*walker->prede); if(net.S != net.S_input) tmp.unbounded_locals.erase(net.S_input);
							main_inf << tmp << endl;
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
							//fw_log << "accelerated to " << *p << " based on predecessor " << *walker << endl;
							break;
						}

						walker = walker->prede;
					}
					
					max_depth_checked = max(max_depth_checked, depth);
				}

//				bool maximal_state = true;
//#define DISCARD_NON_MAX
//#ifdef DISCARD_NON_MAX //only for compact output, otherwise probably too slow
//				for(auto& b : Q)
//				{
//					if(*p <= *b)
//						maximal_state = false, fw_log << "ignore state; it's covered by " << b << endl;
//				}
//#endif

				p->prede = cur, p->depth = 1 + cur->depth; 
				if(Q.insert(p).second)
					W.push(keyprio_pair(p)); // fw_log << "added" << endl;
				else
					delete p;

			}

		}

		threshold_reached = true;

	}
	catch(std::bad_alloc)
	{
		cerr << fatal("forward exploration reached the memory limit") << endl; 
		exe_state = MEMOUT;
#ifndef WIN32
		disable_mem_limit(); //to prevent bad allocations while printing statistics
#endif
	}

	fw_log << "fw while exit" << endl, fw_log.flush();


	////TODO: remove
	//while(!shared_bw_done)
	//{
	//	boost::this_thread::sleep(boost::posix_time::seconds(2));
	//}



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
		/*finish_time = boost::posix_time::microsec_clock::local_time(), */fw_log << "fw first" << endl, fw_log.flush();
	else 
		fw_log << "fw not first" << endl, fw_log.flush();
	
	if(tree_type != GTYPE_NONE) print_dot_search_graph(Q);

	{
		fw_stats << endl;
		fw_stats << "---------------------------------" << endl;
		fw_stats << "Forward statistics:" << endl;
		fw_stats << "---------------------------------" << endl;
		fw_stats << "fw finished first               : " << (shared_fw_finised_first?"yes":"no") << endl;
		fw_stats << "fw execution state              : "; 
		switch(exe_state){
		case TIMEOUT: fw_stats << "TIMEOUT" << endl; break;
		case MEMOUT: fw_stats << "MEMOUT" << endl; break;
		case RUNNING: fw_stats << "RUNNING" << endl; break;
		case INTERRUPTED: fw_stats << "INTERRUPTED" << endl; break;
		}
		fw_stats << endl;
		fw_stats << "forward-reachable global states : " << Q.size() << endl;
		fw_stats << "total forward time              : " << (((float)(boost::posix_time::microsec_clock::local_time() - fw_start_time).total_microseconds())/1000000) << endl;
		fw_stats << endl;
		fw_stats << "projections found               : " << D->size() << endl;
		fw_stats << "time to find projections        : " << (((float)(last_new_prj_found - fw_start_time).total_microseconds())/1000000) << endl;
		fw_stats << "width of last projections       : " << fw_last_prj_width << endl;
		fw_stats << "states at last projections      : " << fw_last_prj_states << endl;
		fw_stats << endl;
		fw_stats << "accelerations                   : " << accelerations << endl;
		fw_stats << "max acceleration depth checked  : " << max_depth_checked << endl;
		fw_stats << "max acceleration depth          : " << max_accel_depth << endl;
		fw_stats << "---------------------------------" << endl;
		fw_stats << "max. forward depth checked      : " << ctr_fw_maxdepth << endl;
		fw_stats << "max. forward width checked      : " << ctr_fw_maxwidth << endl;
		fw_stats << "---------------------------------" << endl;
		fw_stats << "state blocked                   : " << (fw_state_blocked?"yes":"no") << endl;
		fw_stats << "---------------------------------" << endl;
		fw_stats.flush();
	}

	foreach(ostate_t p, Q) //contains init_p
		delete p;
	
	return;
}
#endif
