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
	
	string out_fn = net.filename + ".fw-tree.dot";

	set<ostate_t> seen;
	stack<ostate_t> S;

	ofstream out(out_fn.c_str());
	if(!out.good()) throw logic_error((string)"cannot write to " + out_fn);
	
	out << "digraph BDD {" << std::endl;
	foreach(ostate_t s, Q)
	{
		if(s->prede == nullptr)
			out << "{rank=source; " << '"' << s->str_id() << '"' << '}' << endl;

		out << '"' << s->str_id() << '"' << ' ' << '[' << "lblstyle=" << '"' << "kmnode" << '"' << ",texlbl=" << '"' << s->str_latex() << '"' << ']' << ';' << endl;

		if(s->prede != nullptr)
			if(s->tsucc)
				switch(graph_type){
				case GTYPE_DOT: out << '"' << s->prede->str_id() << '"' << " -> " << '"' << s->str_id() << '"' << "[style=dashed];" << std::endl; break;
				case GTYPE_TIKZ: out << '"' << s->prede->str_id() << '"' << " -> " << '"' << s->str_id() << '"' << "[style=transfer_trans];" << std::endl; break;}
			else
				switch(graph_type){
				case GTYPE_DOT: out << '"' << s->prede->str_id() << '"' << " -> " << '"' << s->str_id() << '"' << ';' << std::endl; break;
				case GTYPE_TIKZ: out << '"' << s->prede->str_id() << '"' << " -> " << '"' << s->str_id() << '"' << "[style=thread_trans];" << std::endl; break;}

		if(s->accel != nullptr)
			switch(graph_type){
				case GTYPE_DOT: out << '"' << s->str_id() << '"' << " -> " << '"' << s->accel->str_id() << '"' << " [" << accel_edge_style << "];" << std::endl; break;
				case GTYPE_TIKZ: out << '"' << s->str_id() << '"' << " -> " << '"' << s->accel->str_id() << '"' << "[style=accel_edge];" << std::endl; break;}
	}

	out << "}" << std::endl;
	out.close();

}

typedef vector<pair<Net::adj_t::const_iterator, trans_type> > tt_list_t;

Oreached_t Post(ostate_t ag, const Net& n, lowerset_vec& D, shared_cmb_deque_t& shared_cmb_deque, bool forward_projections)
{ 
	Oreached_t succs;

	stack<OState*> work;

	work.push(const_cast<OState*>(ag));

	while(!work.empty())
	{
		OState* g = work.top(); work.pop();
		bool g_in_use = g == ag;

		invariant(iff(g == ag,n.core_shared(g->shared)));

		//collect all relevant transitions
		tt_list_t tt_list;
		
		Net::adj_t::const_iterator tc;
		for(auto l = g->bounded_locals.begin(), le = g->bounded_locals.end(); l != le; ++l)
			if((tc = n.adjacency_list.find(Thread_State(g->shared, *l))) != n.adjacency_list.end()) 
				tt_list.push_back(make_pair(tc, thread_transition));
		for(auto l = g->unbounded_locals.begin(), le = g->unbounded_locals.end(); l != le; ++l)
			if((tc = n.adjacency_list.find(Thread_State(g->shared, *l))) != n.adjacency_list.end()) 
				tt_list.push_back(make_pair(tc, thread_transition));
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
				debug_assert(!bounded_in_u || !unbounded_in_u);
				const bool bounded_in_v = g->bounded_locals.find(v.local) != g->bounded_locals.end();
				const bool unbounded_in_v = g->unbounded_locals.find(v.local) != g->unbounded_locals.end();
				const bool thread_in_u = bounded_in_u || unbounded_in_u;
				const bool horiz_trans = (u.shared == v.shared);
				const bool diff_locals = (u.local != v.local);

				if((!thread_in_u && (ty == thread_transition || horiz_trans)))
				{
					//fwinfo << "Ignore: No/no incomparable successors" << endl;
					continue;
				}

				OState* succ;
				if(!g_in_use && ++(ttnex = tt) == tte && ++(tsbnex = tsb) == tse)
				{
					//fwinfo << "Reuse existing state " << *g << endl;
					succ = g, g_in_use = true;
				}
				else
					succ = new OState(g->shared,g->bounded_locals.begin(),g->bounded_locals.end(),g->unbounded_locals.begin(),g->unbounded_locals.end()); //note: even in this case the there might not exist another successor (the continue above could happen)

				succ->tsucc = (g != ag && g->tsucc) || ty == transfer_transition;

				if(!horiz_trans) 
					succ->shared = v.shared;

				if(diff_locals && thread_in_u && !(ty == thread_transition && unbounded_in_u && unbounded_in_v))
				{
					if(ty == thread_transition)
					{
						//replace one bounded occurence of u.local by v.local resp. add one bounded occurence of v.local
						if(bounded_in_u) succ->bounded_locals.erase(succ->bounded_locals.find(u.local)); 
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
				debug_assert(succ->consistent());
				unsigned c = 0;
				if(n.core_shared(succ->shared))
				{
					succs.insert(succ); //add to return set
					c = D.project_and_insert(*succ, shared_cmb_deque, forward_projections); //project
					//cout << c << " ";
				}
				else
				{
					if(net.prj_all) c = D.project_and_insert(*succ, shared_cmb_deque, forward_projections); //project if requested
					work.push(succ); //add to work list
				}

				if(c) 
					last_new_prj_found = boost::posix_time::microsec_clock::local_time();
			}
		}

		if(!g_in_use)
			delete g;
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

	statsout << "Maximum bucket size             : " << max_bucket_size << endl;
	statsout << "Hash load factor                : " << Q.load_factor() << endl;
	statsout << "Max collisions                  : " << max_bucket_size << endl;

	for(map<unsigned, unsigned>::const_iterator i = bucket_size_counters.begin(), ie = bucket_size_counters.end(); i != ie; ++i)
		if(i->first < 9) statsout << "Buckets of size " << boost::lexical_cast<string>(i->first) << "               : " << i->second << endl;
		else statsout << "Buckets of size >" << boost::lexical_cast<string>(i->first) << "              : " << i->second << endl;
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

unsigned fw_qsz = 0
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

void do_fw_bfs(const Net* n, unsigned ab, lowerset_vec* D, shared_cmb_deque_t* shared_cmb_deque, bool forward_projections, unordered_priority_set<ostate_t>::order_t forder)
{
	const bool plain_km = false;
	fw_start_time = boost::posix_time::microsec_clock::local_time();

	Oreached_t				Q;
	OState_priority_queue_t W(forder);
	
	try
	{

		ostate_t init_p;
		init_p = new OState(n->init), Q.insert(init_p), W.push(keyprio_pair(init_p)), D->project_and_insert(*init_p,*shared_cmb_deque,false); //create the initial state and insert it into the reached, work and projection set 
		invariant(init_p->prede == nullptr && init_p->accel == nullptr && init_p->tsucc == false);
		
		ostate_t cur, walker;
		unsigned depth;
		
		while(execution_state == RUNNING && fw_safe && !shared_bw_done && W.try_get(cur)) //retrieve the next working element
		{

			++f_its;
			
			fwinfo << "Exploring: " << *cur << " (depth: " << cur->depth << ")" << endl;
			ctr_fw_maxdepth = max(ctr_fw_maxdepth,cur->depth), ctr_fw_maxwidth = max(ctr_fw_maxwidth,(unsigned)cur->size());

			fw_qsz = Q.size();

			foreach(ostate_t p_c, Post(cur, *n, *D, *shared_cmb_deque, forward_projections)) //allocates objects in "successors"
			{
				OState* p = const_cast<OState*>(p_c); invariant(p->accel == nullptr);

				//fwinfo << "Checking successor " << *p << endl;

				if(n->check_target && n->Otarget <= *p)
				{
					fw_safe = false;
					cerr << "Forward trace to target covering state found; target " << *n->target << " is coverable" << endl;

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
							//fwinfo << "accelerated to " << *p << " based on predecessor " << *walker << endl;
							break;
						}

						walker = walker->prede;
					}
					
					max_depth_checked = max(max_depth_checked, depth);
				}

				p->prede = cur, p->depth = 1 + cur->depth; 
				if(Q.insert(p).second)
					W.push(keyprio_pair(p));/*, fwinfo << "added" << endl;*/
				else
					delete p;
			}

		}
	
	}
	catch(std::bad_alloc)
	{
		cerr << fatal("forward exploration reached the memory limit") << endl; 
		execution_state = MEMOUT;
#ifndef WIN32
		disable_mem_limit(); //to prevent bad allocations while printing statistics
#endif
	}

	shared_fw_done = true;
	
	shared_cout_mutex.lock(), (shared_bw_done) || (shared_fw_finised_first = 1), shared_cout_mutex.unlock();
	
	debug_assert(implies(shared_fw_finised_first,W.empty()));
	
	if(shared_fw_finised_first) 
		finish_time = boost::posix_time::microsec_clock::local_time(), fwinfo << "fw first" << endl;
	else 
		fwinfo << "fw not first" << endl;
	
	if(graph_type != GTYPE_NONE) print_dot_search_graph(Q);

	{
		shared_cout_mutex.lock();

		statsout << endl;
		statsout << "---------------------------------" << endl;
		statsout << "Forward statistics:" << endl;
		statsout << "---------------------------------" << endl;
		statsout << "fw finished first               : " << (shared_fw_finised_first?"yes":"no") << endl;
		statsout << endl;
		statsout << "time to find projections        : " << (((float)(last_new_prj_found - fw_start_time).total_microseconds())/1000000) << endl;
		statsout << "forward-reachable global states : " << Q.size() << endl;
		statsout << "projections found               : " << D->size() << endl;
		statsout << endl;
		statsout << "accelerations                   : " << accelerations << endl;
		statsout << "max acceleration depth checked  : " << max_depth_checked << endl;
		statsout << "max acceleration depth          : " << max_accel_depth << endl;
		if(print_hash_info) statsout << endl, print_hash_stats(Q);
		statsout << "---------------------------------" << endl;
		statsout << "max. forward depth checked      : " << ctr_fw_maxdepth << endl;
		statsout << "max. forward width checked      : " << ctr_fw_maxwidth << endl;
		statsout << "---------------------------------" << endl;

		shared_cout_mutex.unlock();
	}

	foreach(ostate_t p, Q) //contains init_p
		delete p;

	return;
}
#endif
