/******************************************************************************
Synopsis		[Thread Transition System representation.]

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

#include "net.h"

#include <fstream>
#include <stack>

#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/assign/std/vector.hpp>

#ifdef USE_STRONG_COMPONENT
//TODO: remove unnecessary ones
#include <boost/config.hpp>
#include <boost/utility.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/topological_sort.hpp>
#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/dijkstra_shortest_paths.hpp>
#include <boost/graph/visitors.hpp>
#include <boost/graph/graph_utility.hpp>
#include <boost/graph/strong_components.hpp>
#endif

/********************************************************
Net
********************************************************/
Net::Net(): reduce_log(cerr.rdbuf())
{
}

Net::Net(const Net & other): reduce_log(cerr.rdbuf())
{
	assert(0);
}

Net::Net(shared_t S_, local_t L_, OState init_, BState target_, adj_t adjacency_list_, adj_t adjacency_list_inv_, bool prj_all)
	: S_input(-1), L_input(-1), S(S_), L(L_), init(init_), target(target_), adjacency_list(adjacency_list_), adjacency_list_inv(adjacency_list_inv_), reduce_log(cerr.rdbuf())
{ 

	//TODO: this does not check whether adjacency_list_inv is the inverse of adjacency_list; better: build adjacency_list_inv here
	for(auto& a : adjacency_list)
		for(auto& b : a.second)
			for(auto c = b.second.begin(); c != b.second.end(); )
				if(c->second.size() == 1 && c->second.find(c->first) != c->second.end()) c = b.second.erase(c); //remove 0 ~> 0 since 0 not~> l for all l != 0
				else ++c; //keep 0 ~> 0, since 0 ~> l for some l != 0

	core_shared = get_core_shared(prj_all);
}

Net::Net(string net_fn, string target_fn, string init_fn, bool prj_all): filename(net_fn), targetname(target_fn), initname(init_fn), reduce_log(cerr.rdbuf())
{
	
	//parse intial state
	if(init_fn == string())
	{
		init = OState(0), init.unbounded_locals.insert(0);
	}
	else
	{
		try{ 
			init = OState(init_fn); 
		}catch(...){
			ifstream init_in(init_fn.c_str());
			if(!init_in.good()) throw logic_error("cannot read from target input file");
			try{ string tmp; init_in >> tmp; init = OState(tmp); }
			catch(...){ throw logic_error("invalid initial file (only one initial state in the first line supported)"); }
		}
	}

	//parse target state
	if(target_fn == string())
	{
		target.type = BState::invalid;
	}
	else
	{
		try{ 
			target = BState(target_fn,false); 
		}catch(...){
			ifstream target_in(target_fn.c_str());
			if(!target_in.good()) throw logic_error("cannot read from target input file");
			try{ string tmp; target_in >> tmp; target = BState(tmp); }
			catch(...){ throw logic_error("invalid target file (only one target state in the first line supported)"); }
		}
	}

	//parse net
	string line;
	try
	{
		ifstream orig_file(net_fn.c_str());
		istream orig(std::cin.rdbuf()); 

		if(net_fn != "")
		{
			if(!orig_file.is_open()) throw runtime_error((string("cannot read from input file ")).c_str());
			orig.rdbuf(orig_file.rdbuf());
		}

		stringstream in;
		string line2;
		while (getline(orig, line2 = "")) 
		{
			const size_t comment_start = line2.find("#");
			in << ( comment_start == string::npos ? line2 : line2.substr(0, comment_start) ) << endl; 
		}

		in >> S_input >> L_input;
		S = S_input, L = L_input;

		if(S_input == 0 || L_input == 0) throw logic_error("Input file has invalid dimensions");

		trans_type ty;
		unsigned int id = 0;
		while(in)
		{
			getline (in,line);

			boost::tokenizer<boost::char_separator<char> > tok(line, boost::char_separator<char>(" "));
			boost::tokenizer<boost::char_separator<char> >::iterator i = tok.begin();

			//decode transition
			if(i == tok.end()) continue;
			shared_t s1 = boost::lexical_cast<shared_t>(*(i++));
			if(i == tok.end()) throw logic_error("source local state missing");
			local_t l1 = boost::lexical_cast<local_t>(*(i++));
			if(i == tok.end()) throw logic_error("transition separator missing");

			string sep = *(i++);
			if(sep == "->") 
				ty = thread_transition;
			else if(sep == "~>") 
				ty = transfer_transition;
			else if(sep == "+>") 
				ty = spawn_transition;
			else throw logic_error("invalid transition separator");

			if(i == tok.end()) throw logic_error("target shared state missing");
			shared_t s2 = boost::lexical_cast<shared_t>(*(i++));
			if(i == tok.end()) throw logic_error("target local state missing");
			local_t l2 = boost::lexical_cast<local_t>(*(i++));

			Thread_State 
				source(s1, l1), 
				target(s2, l2);

			//decode passive transfer (optional)
			transfers_t transfers, transfers_inv;
			while(i != tok.end())
			{
				local_t l,lp;
				if(ty != thread_transition) throw logic_error("invalid syntax");

				l = boost::lexical_cast<local_t>(*(i++)); 
				if(i == tok.end()) throw logic_error("transfer separator missing");

				if(*(i++) != "~>") throw logic_error("invalid transfer separator");

				if(i == tok.end()) throw logic_error("transfer target missing");
				lp = boost::lexical_cast<local_t>(*(i++));

				if(l >= L_input || lp >= L_input)
					throw logic_error("invalid source or target transfer state");

				transfers[l].insert(lp), transfers_inv[lp].insert(l);
			}

			if(s1 == s2 && l1 == l2)
			{
				if(transfers == transfers_t()) reduce_log << "self-loop ignored" << endl;
				else throw logic_error("self-loops with side-effects on passive threads not allowed");
			}

			if(source.local >= L_input || source.shared >= S_input || target.local >= L_input || target.shared >= S_input)
				throw logic_error("invalid source or target thread state");

			switch(ty)
			{
			case thread_transition:
				{
					adjacency_list[source][target]=transfers, adjacency_list_inv[target][source]=transfers_inv;
				}
				break;
			case transfer_transition:
				{
					if(!transfers.empty()) throw logic_error("passive transfers no permitted in transfer transitions");
					//s1   l1 ~> s2  l2 == s1   p  -> s2   p l1 ~> l2
					if(L == L_input) init.unbounded_locals.insert(L++); //reserve local for thread pool (to rewrite transfer/spawn transitions)
					Thread_State t1(source.shared,L-1), t2(target.shared,L-1);
					adjacency_list[t1][t2][source.local].insert(target.local), adjacency_list_inv[t2][t1][target.local].insert(source.local);
				}
				break;
			case spawn_transition:
				{
					if(!transfers.empty()) throw logic_error("passive transfers no permitted in spawn transitions");
					//s1   l1 +> s2  l2 == s1   l1 -> is  l1, is   p  -> s2  l2
					if(L == L_input) init.unbounded_locals.insert(L++); //reserve local for thread pool (to rewrite transfer/spawn transitions)
					shared_t is = S++;

					Thread_State 
						source1(s1, l1), 
						target1(is, l1),
						source2(is, L-1), 
						target2(s2, l2);

					adjacency_list[source1][target1]=transfers_t(), adjacency_list_inv[target1][source1]=transfers_t();
					++id;
					adjacency_list[source2][target2]=transfers_t(), adjacency_list_inv[target2][source2]=transfers_t();
				}
				break;
			}
			++id;
		}
	}
	catch(bad_cast&)
	{
		throw logic_error((string)"error while parsing line " + '"' + line + '"' + "(bad lexical cast)");
	}
	catch(logic_error& e)
	{
		throw logic_error((string)"error while parsing input file: " + string(e.what()) + " (line: " + line + ")");
	}
	catch(exception& e)
	{
		throw logic_error((string)"could not parse input file (are the line endings correct?): " + e.what());
	}

	core_shared = get_core_shared(prj_all);

}

Net::s_adjs_t Net::get_backward_adj() const
{
	s_adjs_t ret;

	for(auto& a : adjacency_list)
	{
		for(auto& b : a.second)
		{
			const Thread_State& source = a.first, &target = b.first;
			auto f = adjacency_list_inv.find(target);
			auto g = f->second.find(source);
			const transfers_t& transfers = b.second;
			const transfers_t& transfers_inv = g->second;
			ret[target.shared].insert(Transition(source,target,transfers));
		}
	}

	return ret;
}

pair<Net::vv_adjs_t,Net::vvl_adjs_t> Net::get_backward_adj_2() const
{
	
	vv_adjs_t X(S);
	vvl_adjs_t Y(S,vvl_adjs_t::value_type(L));

	for(auto& a : adjacency_list)
	{
		for(auto& b : a.second)
		{
			const Thread_State& u = a.first;
			const Thread_State& v = b.first;
			const transfers_t&	tb = adjacency_list_inv.at(v).at(u);
			const transfers_t&	tf = adjacency_list.at(u).at(v);
			
			if(u.shared != v.shared || !tb.empty())
			{
				X[v.shared].push_back(Transition(u,v,tb,tf)); //diagonal and non-plain transitions 
			}
			else
			{
				Y[v.shared][v.local].push_back(Transition(u,v,tb,tf)); //horizontal plain transitions
			}
		}
	}

	return make_pair(X,Y);

}

vector<bool> Net::get_core_shared(bool prj_all, bool ignore_unused_only) const
{
	vector<bool> ret(S,1);

	if(!prj_all)
	{
		shared_counter_map_t 
			from_shared_counter,
			to_shared_counter,
			from_to_shared_counter;

		for(auto& a : adjacency_list)
		{
			for(auto& b : a.second)
			{
				const Thread_State& source = a.first, &target = b.first;
				auto f = adjacency_list_inv.find(target);
				auto g = f->second.find(source);
				const transfers_t& transfers = b.second;
				const transfers_t& transfers_inv = g->second;

				if(source.shared == target.shared) from_to_shared_counter[source.shared]++;
				else from_shared_counter[source.shared]++, to_shared_counter[target.shared]++;
			}
		}

		for(shared_t s = 0; s < S; ++s)
		{
			auto q = from_to_shared_counter.find(s), f = from_shared_counter.find(s), t = to_shared_counter.find(s);

			ret[s] = 
				(s == init.shared) ||
				(target.type != BState::invalid && s == target.shared) ||
				(ignore_unused_only && ((q != from_to_shared_counter.end() && q->second != 0) || (f != from_shared_counter.end() && f->second != 0) || (t != to_shared_counter.end() && t->second != 0))) || //no incoming or outgoing transitions (neither horizonal nor diagonal ones)
				(!ignore_unused_only && ((q != from_to_shared_counter.end() && q->second != 0) || ((f != from_shared_counter.end() || t != to_shared_counter.end()) && (f == from_shared_counter.end() || t == to_shared_counter.end() || f->second != 1 || t->second != 1)))) //no horizonal transitions (e.g. 2,2 -> 2,5) and exactly one incoming and one outgoing edge
				;
		}
	}

	return ret;
}

set<local_t> operator- (const set<local_t>& L, const unsigned& d)
{
	set<local_t> ret;
	for_each(L.begin(),L.end(),[&ret,&d](const local_t& x){ ret.insert(x-d); });
	return ret;
}

using namespace boost::assign;
void Net::reduce(bool prj_all)
{

	//compute local states that equivalent wrt. to passive effects
	typedef vector<transfers_t> vt_t;
	typedef vector<vt_t> vvt_t;

	vvt_t Q(S);
	for(auto& a : adjacency_list)
	{
		for(auto& b : a.second)
		{
			const Thread_State& source = a.first;
			const transfers_t& transfers = b.second;
			if(transfers != transfers_t())
				Q[source.shared].push_back(transfers);
		}
	}

	map<local_t,map<local_t,bool> > EQl;
	for(local_t l = 0; l < L; ++l)
	{
		for(local_t lp = l; lp < L; ++lp)
		{
			//reduce_log << l << "==" << lp << "?";
			EQl[l][lp] = EQl[lp][l] = 
				l == lp ||
				all_of(Q.begin(),Q.end(),[&l,&lp](const vt_t& q){ return 
				all_of(q.begin(),q.end(),[&l,&lp](const transfers_t& t) { return 
				t.find(l) == t.end() && t.find(lp) == t.end() ||
				(t.find(l) != t.end() && t.find(lp) != t.end() && t.at(l)-l == t.at(lp)-lp); });});
			//reduce_log << EQl[l][lp] << endl;
		}
	}

	//compute core local states
	lls_map_t
		lcur_lnex_to_shared,
		l_to_s_inv
		;

	locals_boolvec_t
		trans_from_local,
		trans_to_local,
		diag_trans_to_local,
		non_plain_from_local,
		non_plain_to_local,
		non_eq_transitions_to_local,
		is_transferred_from,
		is_transferred_to
		;

	for(auto& a : adjacency_list)
	{
		for(auto& b : a.second)
		{
			const Thread_State& source = a.first, &target = b.first;
			const transfers_t& transfers = b.second, &transfers_inv = adjacency_list_inv[target][source];

			lcur_lnex_to_shared[source.local][target.local].insert(target.shared);
			l_to_s_inv[target.local][source.local].insert(target.shared);
			trans_from_local[source.local] = true, trans_to_local[target.local] = true;

			diag_trans_to_local[target.local] |= source.shared != target.shared;
			non_plain_from_local[source.local] |= !transfers.empty(), non_plain_to_local[target.local] |= !transfers.empty();
			non_eq_transitions_to_local[target.local] |= !EQl[source.local][target.local];

			for(auto& pa : b.second)
			{
				is_transferred_from[pa.first] = true;
				for(auto& pb : pa.second)
				{
					is_transferred_to[pb] = true;
				}
			}			
		}
	}

	vector<bool> core_shared = get_core_shared();

	reduce_log << "core shared: ";
	for(shared_t s = 0; s < S; ++s)
		if(core_shared[s]) reduce_log << s << " ";
	reduce_log<< endl;

	unsigned matr = 0;
	bool sound_por = false;
	//bool sound_por = true;
	if(sound_por) //to determine whether transitions are shared-state independent
		matr = std::count_if(core_shared.begin(), core_shared.end(), [](bool i){return i;});
	else //this is correct for satabs generated nets but may overapproximate for other models
		for_each(l_to_s_inv.begin(),l_to_s_inv.end(),[&matr,&l_to_s_inv](lls_map_t::value_type& x){
			for_each(x.second.begin(),x.second.end(),[&matr,&l_to_s_inv](ls_m_t::value_type& y){
				matr = max(matr,(unsigned)y.second.size());});});

	vector<bool> core_local(L);

	scmap_t V;
	for(local_t l = 0; l < L; ++l)
	{
		bool b = trans_from_local[l] || trans_to_local[l];
		core_local[l] =
			COUNT_IF(V,"0INITB",init.bounded_locals.find(l) != init.bounded_locals.end()) || //is a bounded initial local state
			COUNT_IF(V,"1INITU",init.unbounded_locals.find(l) != init.unbounded_locals.end())|| //is an unbounded initial local state
			COUNT_IF(V,"2TARGE",target.bounded_locals.find(l) != target.bounded_locals.end())|| //is a target local state
			COUNT_IF(V,"3BEEND",b && (trans_from_local[l] != trans_to_local[l]))|| //beginning or end of a transition sequence
			COUNT_IF(V,"4DIAGI",b && (diag_trans_to_local[l]))|| //no diagonal transition enters the local state (outgoing diagonal transitions are fine)
			COUNT_IF(V,"5NONPO",b && (non_plain_from_local[l]))|| //one or more outgoing edges are no plain (i.e.|| with no effect on passive threads) thread transitions
			COUNT_IF(V,"6NONPI",b && (non_plain_to_local[l]))|| //one or more ingoing edges are no plain thread transitions
			COUNT_IF(V,"7SINDE",b && (any_of(l_to_s_inv[l].begin(),l_to_s_inv[l].end(),[&matr,this](ls_m_t::value_type& x){ reduce_log << x.second.size() << "/" << matr << endl; return x.second.size() < matr; })))|| //transition is independent of the shared state
			COUNT_IF(V,"8PASSE",b && (non_eq_transitions_to_local[l])) || //relative passive effects are the same on all predecessors as on l
			COUNT_IF(V,"9SINKK",!b && (!is_transferred_from[l] && is_transferred_to[l])) //can only be reached passively (TODO: all sink states could be merged to further reduce the net)
			;
	}
	reduce_log << endl;

	for_each(V.begin(),V.end(),[this](scmap_t::value_type& c){ reduce_log << c.first << ": " << c.second << endl; });

	unsigned core_num = 0;
	core_num = std::count_if(core_local.begin(),core_local.end(),bind2nd(equal_to<bool>(),true));

	reduce_log << "total local core states: " << core_num << "/" << L << endl;

	reduce_log << "non-core local: ";
	for(local_t l = 0; l < L; ++l)//if(!core_local[l]) reduce_log << l << " ";
		if(!core_local[l]) reduce_log << l << "(" << is_transferred_from[l] << is_transferred_to[l] << ")" << " ";
	reduce_log<< endl;

	//compute new local state values
	local_perm_t p(L,-1);
	local_t L_new = 0;

	for(local_t l = 0; l < L; ++l)
		if(core_local[l]) p[l] = L_new++, reduce_log << p[l] << "/" << l << " ";
		else reduce_log << "-" << "/" << l << " ";
	reduce_log << endl;

	//compute new shared state values
	vector<bool> used_shared = get_core_shared(prj_all,true);
	local_perm_t pS(S,-1);
	local_t S_new = 0;
	for(shared_t s = 0; s < S; ++s)
		if(used_shared[s]) pS[s] = S_new++, reduce_log << pS[s] << "/" << s << " ";
		else reduce_log << "-" << "/" << s << " ";
	reduce_log << endl;

	//reduce net by removing non-core local states 
	adj_t processed, processed_inv;
	typedef set<pair<Thread_State,Thread_State> > s_t;
	s_t seen;
	stack<s_t::const_iterator> work;

	for(auto s : adjacency_list)
	{
		for(auto t : s.second)
		{
			if(core_local[s.first.local] && core_local[t.first.local])
			{
				reduce_log << s.first << " -> " << t.first << " ";
				transfers_t n, n_inv;
				for(auto b : t.second)
					if(core_local[b.first]) 
						for(auto c : b.second)
							if(core_local[c])
								n[p[b.first]].insert(p[c]), n_inv[p[c]].insert(p[b.first]), reduce_log << b.first << " ~> " << c << " ";
				reduce_log << endl;

				Thread_State
					x(pS[s.first.shared],p[s.first.local]),
					y(pS[t.first.shared],p[t.first.local]);

				processed[x][y]=n, processed_inv[y][x]=n_inv;
			}
			else
			{
				invariant(t.second == transfers_t());
				work.push(seen.insert(make_pair(s.first,t.first)).first);
			}
		}
	}

	unsigned init_work_sz, init_processed_sz = 0, final_processed_sz = 0;
	init_work_sz = work.size(), for_each(processed.begin(), processed.end(), [&init_processed_sz](adj_t::value_type& x) { init_processed_sz += x.second.size(); });

	while(!work.empty())
	{
		Thread_State u = work.top()->first, v = work.top()->second; work.pop();

		reduce_log << "processing " << u << " -> " << v << "\t";

		if(core_local[u.local] && core_local[v.local]) 
		{
			Thread_State
				x(pS[u.shared],p[u.local]),
				y(pS[v.shared],p[v.local]);

			if(x == y)
			{
				reduce_log << "same source and target -> ignore" << endl;
				continue;
			}

			processed[x][y] = transfers_t(), processed_inv[y][x] = transfers_t(), reduce_log << "core" << endl;
		}
		else if(!core_local[u.local] && !core_local[v.local])
			reduce_log << "no core" << endl;
		else
		{
			set<Thread_State> pre_u, post_v;

			if(core_local[u.local]) pre_u.insert(u);
			else if(adjacency_list_inv.find(u) == adjacency_list_inv.end()){ reduce_log << "no pre" << endl; continue; }
			else for( auto& p : adjacency_list_inv[u] ) pre_u.insert(p.first);

			if(core_local[v.local]) post_v.insert(v);
			else if(adjacency_list.find(v) == adjacency_list.end()){ reduce_log << "no post" << endl; continue; }
			else for( auto& p : adjacency_list[v] ) post_v.insert(p.first);

			for(auto p : pre_u) for(auto q : post_v) 
			{
				auto i = seen.insert(make_pair(p,q));
				if(i.second) work.push(i.first), reduce_log << "adding " << p << " -> " << q << " ";
				else reduce_log << "not new ";
			}
			reduce_log << "; " << work.size() << " transitions remaining" << endl;
		}
	}

	for_each(processed.begin(), processed.end(), [&final_processed_sz](adj_t::value_type& x) { final_processed_sz += x.second.size(); });

	reduce_log << "init work: " << init_work_sz << " init processed: " << init_processed_sz << " final processed: " << final_processed_sz << " diff: " << (int)(init_work_sz + init_processed_sz) - (int)final_processed_sz << endl;

	//update initial and target state
	OState init_new(pS[init.shared]);
	for(local_t l : init.bounded_locals) init_new.bounded_locals.insert(p[l]);
	for(local_t l : init.unbounded_locals) init_new.unbounded_locals.insert(p[l]);

	BState target_new(pS[target.shared]);
	for(local_t l : target.bounded_locals) target_new.bounded_locals.insert(p[l]);

	//return
	Net(S_new,L_new,init_new,target_new,processed,processed_inv,prj_all).swap(*this);

}

void Net::swap(Net& other)
{
	if (this != &other)
	{
		std::swap(S_input,other.S_input);
		std::swap(S,other.S);
		std::swap(L_input,other.L_input);
		std::swap(L,other.L);
		init.swap(other.init);
		target.swap(other.target);
		filename.swap(other.filename);
		targetname.swap(other.targetname);
		initname.swap(other.initname);
		adjacency_list.swap(other.adjacency_list);
		adjacency_list_inv.swap(other.adjacency_list_inv);
		reduce_log.swap(other.reduce_log);
		core_shared.swap(other.core_shared);
	}
}

Net::stats_t Net::get_stats(bool sccnetstats) const
{
	stats_t ret;

	ret.S = BState::S, ret.L = BState::L, ret.T = 0;
	set<Thread_State> seen;

	//degree

	map<Thread_State,unsigned> indegree, outdegree;

	for(auto s : adjacency_list)
	{
		for(auto t : s.second)
		{
			auto source = s.first, target = t.first;
			++ret.T,
				seen.insert(source), 
				seen.insert(target),
				++outdegree[source],
				++indegree[target];
		}
	}

	ret.max_indegree = 0, ret.max_outdegree = 0, ret.max_degree = 0;
	foreach(Thread_State t, seen)
	{
		ret.max_indegree = max(indegree[t], ret.max_indegree);
		ret.max_outdegree = max(outdegree[t], ret.max_outdegree);
		ret.max_degree = max(indegree[t] + outdegree[t], ret.max_degree);
	}

	ret.discond = (BState::S * BState::L) - seen.size();

#ifdef USE_STRONG_COMPONENT
	if(sccnetstats)
	{
		//build Graph
		using namespace boost;
		typedef boost::adjacency_list<vecS, vecS, bidirectionalS> Graph;

		Graph g(this->init.L * this->init.S); //argument specified number of vertexes
		foreach(Transition t, trans_list)
			add_edge(t.source.unique_id(OState::S,OState::L),t.target.unique_id(OState::S,OState::L),g);

		//boost::print_graph(g, boost::get(boost::vertex_index, g));
		std::vector<int> component(num_vertices(g)), discover_time(num_vertices(g));
		std::vector<default_color_type> color(num_vertices(g));
		std::vector<unsigned> root(num_vertices(g));
		ret.SCC_num = strong_components(g, &component[0], root_map(&root[0]).color_map(&color[0]).discover_time_map(&discover_time[0]));
	}
#endif

	ret.core_shared_states = 0;
	auto core_shared = get_core_shared();
	for(auto b : core_shared) if(b) ++ret.core_shared_states;

	return ret;

}

ostream& operator << (ostream& out, const Net& n)
{ 
	if(!out.rdbuf()) return out;

	out << "#init   " << n.init << endl;
	out << "#target " << n.target << endl;
	out << n.S << " " << n.L << endl;
	for(auto s : n.adjacency_list) for(auto t : s.second)
	{
		out << s.first << " -> " << t.first << " ";
		for(auto b : t.second) for(auto c : b.second) out << b.first << " ~> " << c << " ";
		out << endl;
	}

	return out;
}
