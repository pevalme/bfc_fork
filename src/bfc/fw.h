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

typedef vector<pair<Net::adj_t::const_iterator, trans_type> > tt_list_t;

Oreached_t Post(ostate_t ag, Net& n, lowerset_vec& D, shared_cmb_deque_t& shared_cmb_deque, bool forward_projections)
{ 
	
	Oreached_t succs;

	stack<OState*> work;

	work.push(const_cast<OState*>(ag));

	fw_log << "computing successors" << "\n";
	while(!work.empty())
	{
		OState* g = work.top(); work.pop();
		bool g_in_use = g == ag;

		invariant(iff(g == ag,n.core_shared(g->shared)));

		//collect all relevant transitions
		tt_list_t tt_list;
		
		Net::adj_t::const_iterator tc;
		
		for(auto l = g->bounded_locals.begin(), le = g->bounded_locals.end();l != le;)
		{
			if((tc = n.adjacency_list.find(Thread_State(g->shared, *l))) != n.adjacency_list.end()) 
				tt_list.push_back(make_pair(tc, thread_transition));
			if((tc = n.spawn_adjacency_list.find(Thread_State(g->shared, *l))) != n.spawn_adjacency_list.end()) 
				tt_list.push_back(make_pair(tc, spawn_transition));

			local_t last = *l;
			while(++l != le && last == *l); //skip same locals
		}
		for(auto l = g->unbounded_locals.begin(), le = g->unbounded_locals.end(); l != le; ++l)
		{
			if((tc = n.adjacency_list.find(Thread_State(g->shared, *l))) != n.adjacency_list.end()) 
				tt_list.push_back(make_pair(tc, thread_transition));
			if((tc = n.spawn_adjacency_list.find(Thread_State(g->shared, *l))) != n.spawn_adjacency_list.end()) 
				tt_list.push_back(make_pair(tc, spawn_transition));
		}
		auto t_transfers = n.transfer_adjacency_list_froms.find(g->shared);
		if(t_transfers != n.transfer_adjacency_list_froms.end())
			for(auto tc = t_transfers->second.begin(), te = t_transfers->second.end(); tc!=te; ++tc)
				tt_list.push_back(make_pair(tc,transfer_transition));

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
				const Thread_State& v = *tsb;
				const bool bounded_in_u = g->bounded_locals.find(u.local) != g->bounded_locals.end();
				const bool unbounded_in_u = g->unbounded_locals.find(u.local) != g->unbounded_locals.end(); 
				invariant(!bounded_in_u || !unbounded_in_u);
				const bool bounded_in_v = g->bounded_locals.find(v.local) != g->bounded_locals.end();
				const bool unbounded_in_v = g->unbounded_locals.find(v.local) != g->unbounded_locals.end();
				const bool thread_in_u = bounded_in_u || unbounded_in_u;
				const bool horiz_trans = (u.shared == v.shared);
				const bool diff_locals = (u.local != v.local);

				//if((!thread_in_u && (ty == thread_transition || horiz_trans)))
				if((!thread_in_u && (ty == thread_transition || ty == spawn_transition || horiz_trans)))
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

				//if(diff_locals && thread_in_u && !(ty == thread_transition && unbounded_in_u && unbounded_in_v))
				if(diff_locals && thread_in_u && !((ty == spawn_transition || ty == thread_transition) && unbounded_in_u && unbounded_in_v))
				{
					if(ty == thread_transition || ty == spawn_transition)
					{
						//replace one bounded occurence of u.local by v.local resp. add one bounded occurence of v.local
						if(ty == thread_transition && bounded_in_u) succ->bounded_locals.erase(succ->bounded_locals.find(u.local)); 
						if(!unbounded_in_v) succ->bounded_locals.insert(v.local);
					}
					else
					{
						if(bounded_in_u)
						{
							//remove all occurences of u.local and, if necessary, add v.local for each
							size_t removed = succ->bounded_locals.erase(u.local); 
							if(!unbounded_in_v) while(removed--) succ->bounded_locals.insert(v.local);
						}
						else
						{
							//remove the unbounded occurence of u.local and, if necessary, add v.local instead
							succ->unbounded_locals.erase(succ->unbounded_locals.find(u.local)); 
							if(!unbounded_in_v) succ->unbounded_locals.insert(v.local);
							if(bounded_in_v) succ->bounded_locals.erase(v.local);
						}
					}
				}
				invariant(succ->consistent());
				
				unsigned c = 0;
				if(n.core_shared(succ->shared))
				{
					fw_log << "Found core successor: " << *succ << "\n";

					succs.insert(succ); //add to return set
					c = D.project_and_insert(*succ, shared_cmb_deque, forward_projections); //project
					//fw_log << c << " ";
				}
				else
				{
					fw_log << "Found intermediate successor: " << *succ << "\n";

					if(net.prj_all) c = D.project_and_insert(*succ, shared_cmb_deque, forward_projections); //project if requested
					work.push(succ); //add to work list
				}

				if(c) 
					last_new_prj_found = boost::posix_time::microsec_clock::local_time(), 
					fw_prj_found += c, 
					fw_last_prj_width = succ->size();
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
				Oreached_t resolved_state(p_c->resolve_omegas(max_fw_width-1)); //all states with width max_fw_width are blocked anyway
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

		
		
		multiset<local_t> lll;
		//lll.insert(0);
		//lll.insert(0);
		lll.insert(0);
		lll.insert(0);
		lll.insert(61);
		//cur = new OState("0|0,0,0,61");
		cur = new OState(0,lll.begin(),lll.end());
		Post(cur, *n, *D, *shared_cmb_deque, forward_projections);



		while(execution_state == RUNNING && fw_safe && (!shared_bw_done || !bw_safe) && W.try_get(cur)) //retrieve the next working element
		{

			++f_its;
			
			fw_log << "Exploring: " << *cur << " (depth: " << cur->depth << ")" << "\n";
			ctr_fw_maxdepth = max(ctr_fw_maxdepth,cur->depth), ctr_fw_maxwidth = max(ctr_fw_maxwidth,(unsigned)cur->size());

			fw_qsz = Q.size();
			fw_wsz = W.size();

			//if(fw_threshold != OPT_STR_FW_THRESHOLD_DEFVAL/* && f_its%1000==0*/) //just do this from time to time
			if(fw_threshold != OPT_STR_FW_THRESHOLD_DEFVAL && f_its > 4) //just do this from time to time
			{
				auto diff = (boost::posix_time::microsec_clock::local_time() - last_new_prj_found);
				if((diff.hours() * 3600 + 60 * diff.minutes() + diff.seconds()) > fw_threshold)
					threshold_reached = true;
			}
			
			static bool first = true;
			if(first && threshold_reached)
				fw_log << "threshold reached" << "\n", first = false;

			foreach(ostate_t p_c, Post(cur, *n, *D, *shared_cmb_deque, forward_projections)) //allocates objects in "successors"
			{
				
				if((threshold_reached) || (max_fw_width != OPT_STR_FW_WIDTH_DEFVAL && p_c->size() >= max_fw_width)) //TEST
				{
					fw_log << *p_c << " blocked" << "\n";
					fw_state_blocked = true;
					delete p_c;
					continue;
				}

				OState* p = const_cast<OState*>(p_c); invariant(p->accel == nullptr);

				fw_log << "checking successor " << *p << "\n";

				if(n->check_target && n->Otarget <= *p)
				{
					fw_safe = false;
					fw_log << "Forward trace to target covering state found" << "\n";

					//print trace
					ostate_t walker = cur;
					
					OState tmp(*p); tmp.unbounded_locals.erase(net.local_thread_pool);
					main_inf << "counterexample" << "\n";
					main_inf << tmp << "\n";

					if(walker->shared < net.S_input){
						OState tmp(*walker); tmp.unbounded_locals.erase(net.local_thread_pool);
						main_inf << tmp << "\n";
					}
					while(walker->prede != nullptr)
					{
						if(walker->prede->shared < net.S_input) 
						{
							OState tmp(*walker->prede); tmp.unbounded_locals.erase(net.local_thread_pool);
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

			fw_log.flush();

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
		case RUNNING: bw_stats << "RUNNING" << "\n"; break;
		case INTERRUPTED: fw_stats << "INTERRUPTED" << "\n"; break;}
		fw_stats << "\n";
		fw_stats << "time to find projections        : " << (((float)(last_new_prj_found - fw_start_time).total_microseconds())/1000000) << "\n";
		fw_stats << "width of last projections       : " << fw_last_prj_width << "\n";
		fw_stats << "forward-reachable global states : " << Q.size() << "\n";
		fw_stats << "projections found               : " << D->size() << "\n";
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
