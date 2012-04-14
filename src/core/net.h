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

#ifndef NET_H
#define NET_H

#include <fstream>
#include <list>
#include <boost/tokenizer.hpp>
#include "ostate.h"
#include "bstate.h"
#include "tstate.h"
#include "trans.h"
#include <map>
#include "types.h"
#include <boost/lexical_cast.hpp>

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

/********************************************************
Net
********************************************************/
struct Net{

	/* ---- Types ---- */	
	enum net_format_t {MIST,TIKZ,TTS,LOLA,TINA,CLASSIFY}; //CLASSIFY is no net type

	typedef map<Thread_State, set<Thread_State> > adj_t;
	typedef map<ushort, adj_t> s_adj_t;
	typedef set<Transition> adjs_t;
	typedef map<ushort, adjs_t> s_adjs_t;
	typedef map<shared_t, unsigned> shared_counter_map_t;
	typedef list<Transition> trans_list_t;
	typedef vector<unsigned> locals_boolvec_t;

	/* ---- Transition relation ---- */	
	trans_list_t 
		trans_list;

	adj_t 
		adjacency_list, 
		inv_adjacency_list, 
		transfer_adjacency_list, 
		inv_transfer_adjacency_list,
		spawn_adjacency_list,
		inv_spawn_adjacency_list;

	s_adj_t 
		adjacency_list_tos, 
		transfer_adjacency_list_tos, 
		transfer_adjacency_list_froms;

	s_adjs_t 
		adjacency_list_tos2;

	shared_counter_map_t 
		from_shared_counter,
		to_shared_counter,
		from_to_shared_counter;

	locals_boolvec_t
		diag_trans_from_local,
		diag_trans_to_local;

	/* ---- Misc members ---- */	
	string filename;

	OState 
		init, 
		Otarget;

	local_t 
		local_init,
		init_local2,
		local_thread_pool;


	BState const * target;

	bool 
		check_target, 
		prj_all,
		has_spawns;

	shared_t S_input;
	local_t L_input;
	
	/* ---- Constructors/input ---- */
	void read_net_from_file(string fn)
	{
		string line;
		try
		{
			filename = fn;
			has_spawns = false;

			ifstream orig_file(filename.c_str());
			istream orig(std::cin.rdbuf()); 

			if(fn != "")
			{
				if(!orig_file.is_open()) 
					throw runtime_error((string("cannot read from input file ") + filename).c_str());
				else 
					maincout << "Reading TTS from file " << filename << "\n", maincout.flush();
				
				orig.rdbuf(orig_file.rdbuf());
			}
			else
			{
				maincout << "Reading TTS from stdin" << "\n", maincout.flush();
			}

			stringstream in;
			string line2;
			while (getline(orig, line2 = "")) 
			{
				const size_t comment_start = line2.find("#");
				in << ( comment_start == string::npos ? line2 : line2.substr(0, comment_start) ) << endl; 
			}
			//orig.close();

			in >> BState::S >> BState::L;
			shared_t smax = BState::S;

			S_input = BState::S;
			L_input = BState::L;

#ifndef NOSPAWN_REWRITE
			local_thread_pool = BState::L;
			BState::L++;
			bool local_thread_pool_inuse = false;
#endif

			shared_t s1, s2;
			local_t l1, l2;
			trans_type ty;

			diag_trans_from_local.resize(BState::L), diag_trans_to_local.resize(BState::L);

			set<Transition> trans_set;;
			map<ushort, unsigned int> in_out_counter;
			unsigned int id = 0;

			Thread_State 
				source,
				target;

			while (in)
			{
				//try{
				getline (in,line);

				boost::tokenizer<boost::char_separator<char> > tok(line, boost::char_separator<char>(" "));
				boost::tokenizer<boost::char_separator<char> >::iterator i = tok.begin();

				//decode transition
				if(i == tok.end()) continue;
				s1 = boost::lexical_cast<shared_t>(*(i++));
				if(i == tok.end()) 
					throw logic_error("source local state missing");
				l1 = boost::lexical_cast<local_t>(*(i++));
				if(i == tok.end()) 
					throw logic_error("transition separator missing");

				string sep = *(i++);
				if(sep == "->") 
					ty = thread_transition;
				else if(sep == "~>") 
					ty = transfer_transition;
				else if(sep == "+>") 
					ty = spawn_transition;
				else 
					throw logic_error("invalid transition separator");

				if(i == tok.end()) 
					throw logic_error("target shared state missing");
				s2 = boost::lexical_cast<shared_t>(*(i++));
				if(i == tok.end()) 
					throw logic_error("target local state missing");
				l2 = boost::lexical_cast<local_t>(*(i++));

				Thread_State 
					source(s1, l1), 
					target(s2, l2);

				if(source.local >= BState::L || source.shared >= smax || target.local >= BState::L || target.shared >= smax){
					throw logic_error("invalid source or target thread state");
				}

				if(!trans_set.insert(Transition(source,target,ty)).second)
				{
					maincout << warning("multiple occurrences of transition ") << Transition(source,target,ty) << "\n", maincout.flush();
					continue;
				}

				switch(ty)
				{
				case thread_transition:
					diag_trans_from_local[source.local] |= s1 != s2;
					diag_trans_to_local[target.local] |= s1 != s2;

					//store transition
					if(s1 == s2)
						from_to_shared_counter[source.shared]++, 
						trans_list.push_back(Transition(source,target,ty,id,hor));
					else
						from_shared_counter[source.shared]++, 
						to_shared_counter[target.shared]++,
						trans_list.push_back(Transition(source,target,ty,id,nonhor))
						;

					adjacency_list[source].insert(target);
					inv_adjacency_list[target].insert(source);

					adjacency_list_tos[target.shared][source].insert(target);
					adjacency_list_tos2[target.shared].insert(Transition(source,target,thread_transition,id));
					break;
				case transfer_transition:
					diag_trans_from_local[source.local] |= s1 != s2;
					diag_trans_to_local[target.local] |= s1 != s2;

					//store transition
					if(s1 == s2)
						from_to_shared_counter[source.shared]++, 
						trans_list.push_back(Transition(source,target,ty,id,hor));
					else
						from_shared_counter[source.shared]++, 
						to_shared_counter[target.shared]++,
						trans_list.push_back(Transition(source,target,ty,id,nonhor))
						;

					transfer_adjacency_list[source].insert(target); 
					transfer_adjacency_list_tos[target.shared][source].insert(target);
					transfer_adjacency_list_froms[source.shared][source].insert(target);
					inv_transfer_adjacency_list[target].insert(source);

					adjacency_list_tos2[target.shared].insert(Transition(source,target,transfer_transition,id));
					break;
				case spawn_transition:
					has_spawns = true; 
#ifndef NOSPAWN_REWRITE

					local_thread_pool_inuse = true;

					/*
					s1   l1 +> s2  l2
					==
					s1   l1 -> is  l1
					is   p  -> s2  l2
					*/

					shared_t is = BState::S++;

					Thread_State 
						source1(s1, l1), 
						target1(is, l1),
						source2(is, local_thread_pool), 
						target2(s2, l2);

					diag_trans_from_local[source1.local] |= source1.shared != target1.shared;
					diag_trans_to_local[target1.local] |= source1.shared != target1.shared;

					//store transition
					if(source1.shared == target1.shared)
						from_to_shared_counter[source1.shared]++, 
						trans_list.push_back(Transition(source1,target1,thread_transition,id,hor));
					else
						from_shared_counter[source1.shared]++, 
						to_shared_counter[target1.shared]++,
						trans_list.push_back(Transition(source1,target1,thread_transition,id,nonhor))
						;

					adjacency_list[source1].insert(target1);
					inv_adjacency_list[target1].insert(source1);
					adjacency_list_tos[target1.shared][source1].insert(target1);
					adjacency_list_tos2[target1.shared].insert(Transition(source1,target1,thread_transition,id));



					++id;
					diag_trans_from_local[source2.local] |= source2.shared != target2.shared;
					diag_trans_to_local[target2.local] |= source2.shared != target2.shared;

					//store transition
					if(source2.shared == target2.shared)
						from_to_shared_counter[source2.shared]++, 
						trans_list.push_back(Transition(source2,target2,thread_transition,id,hor));
					else
						from_shared_counter[source2.shared]++, 
						to_shared_counter[target2.shared]++,
						trans_list.push_back(Transition(source2,target2,thread_transition,id,nonhor))
						;

					adjacency_list[source2].insert(target2);
					inv_adjacency_list[target2].insert(source2);
					adjacency_list_tos[target2.shared][source2].insert(target2);
					adjacency_list_tos2[target2.shared].insert(Transition(source2,target2,thread_transition,id));
#else
					diag_trans_from_local[source.local] |= s1 != s2;
					diag_trans_to_local[target.local] |= s1 != s2;

					//store transition
					if(s1 == s2)
						from_to_shared_counter[source.shared]++, 
						trans_list.push_back(Transition(source,target,ty,id,hor));
					else
						from_shared_counter[source.shared]++, 
						to_shared_counter[target.shared]++,
						trans_list.push_back(Transition(source,target,ty,id,nonhor))
						;

					spawn_adjacency_list[source].insert(target); //used by fw
					inv_adjacency_list[target].insert(source);
					adjacency_list_tos2[target.shared].insert(Transition(source,target,spawn_transition,id)); //used by bw
#endif
					break;
				}
				//}
				//catch(...)
				//{
				//	maincout << "ignore line: " << line << endl;
				//}
				++id;
			}
			if(!local_thread_pool_inuse)
				--BState::L; //only use if additional local state if neccessary
			OState::S = BState::S;
			OState::L = BState::L;

		}
		catch(bad_cast&)
		{
			throw logic_error((string)"error while parsing line " + '"' + line + '"' + "(bad lexical cast)");
		}
		catch(logic_error& e)
		{
			throw logic_error((string)"error while parsing input file: " + string(e.what()));
		}
		catch(exception& e)
		{
			throw logic_error((string)"could not parse input file (are the line endings correct?): " + e.what());
		}
	}
	
	unordered_map<shared_t,bool> core_shared_cache;
	bool core_shared(const shared_t& s)
	{
		bool ret;
		
		auto ch_f = core_shared_cache.find(s);
		Net::shared_counter_map_t::const_iterator q;

		if(ch_f != core_shared_cache.end())
			return ch_f->second;
		else if(prj_all || s == init.shared) 
			ret = true; //otherwise e.g. 0 0 -> 1 1, 1 0 -> 0 1 would have no core state
		else if(target != nullptr && s == target->shared)
			ret = true;
		else if((q = from_to_shared_counter.find(s)) != from_to_shared_counter.end() && q->second != 0)
			ret = true; //for horizonal transitions, e.g. 2,2 -> 2,5
		else
		{
			auto f = from_shared_counter.find(s), t = to_shared_counter.find(s);
			ret = !(f != from_shared_counter.end() && t != to_shared_counter.end() && f->second == 1 && t->second == 1);
		}
		
		core_shared_cache[s] = ret;
		return ret;
	}

	/*
	unordered_map<local_t,bool> core_local_cache;
	bool core_local(const local_t& l)
	{

		return true; //TODO: Das führt bei einigen Bsp. noch zur Nicht-Terminierung!!!

		bool ret;

		auto ch_f = core_local_cache.find(l);

		if(ch_f != core_local_cache.end())
			return ch_f->second;
		else if(prj_all || l == local_init)
			ret = true;
		else if(target != nullptr && target->bounded_locals.find(l) != target->bounded_locals.end())
			ret = true;
		else if(!diag_trans_from_local[l] && !diag_trans_to_local[l])
			ret = false;
		else
			ret = true;

		core_local_cache[l] = ret;
		return ret;

	}
	*/

	/* ---- Statistics ---- */
	struct net_stats_t
	{
		unsigned S,L,T;
		map<trans_type,unsigned> trans_type_counters;
		map<trans_dir_t,unsigned> trans_dir_counters;
#ifdef DETAILED_TTS_STATS
		unsigned SCC_num; //total number of strongly connected components
#endif
		unsigned discond; //number of thread states that have no incoming or outgoing edge
		unsigned max_indegree;
		unsigned max_outdegree;
		unsigned max_degree;
	};
	
	net_stats_t get_net_stats() const
	{
		net_stats_t ret;

		ret.S = BState::S, ret.L = BState::L, ret.T = 0;
		set<Thread_State> seen;

		//degree
		
		map<Thread_State,unsigned> indegree, outdegree;

		foreach(const Transition& t, trans_list)
		{
			++ret.trans_dir_counters[t.dir],
			++ret.trans_type_counters[t.type],
			++ret.T,
			seen.insert(t.source), seen.insert(t.target),
			++outdegree[t.source],
			++indegree[t.target]
			;
		}

		ret.max_indegree = 0, ret.max_outdegree = 0, ret.max_degree = 0;
		foreach(Thread_State t, seen)
		{
			ret.max_indegree = max(indegree[t], ret.max_indegree);
			ret.max_outdegree = max(outdegree[t], ret.max_outdegree);
			ret.max_degree = max(indegree[t] + outdegree[t], ret.max_degree);
		}

		ret.discond = (OState::S * OState::L) - seen.size();

#ifdef DETAILED_TTS_STATS
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
#endif

		return ret;
	}

	~Net()
	{
		//this can cause a segmentation fault
		//BState* r = const_cast<BState*>(target);
		//delete r;
	}

};

#endif