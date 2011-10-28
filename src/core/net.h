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
	enum net_format_t {MIST,TIKZ,TTS,CLASSIFY}; //CLASSIFY is no net type

	typedef map<Thread_State, set<Thread_State> > adj_t;
	typedef map<ushort, adj_t> s_adj_t;
	typedef set<Transition> adjs_t;
	typedef map<ushort, adjs_t> s_adjs_t;
	typedef map<shared_t, unsigned> shared_counter_map_t;
	typedef list<Transition> trans_list_t;

	/* ---- Transition relation ---- */	
	trans_list_t 
		trans_list;

	adj_t 
		adjacency_list, 
		inv_adjacency_list, 
		transfer_adjacency_list, 
		inv_transfer_adjacency_list;

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

	/* ---- Misc members ---- */	
	string filename;

	OState 
		init, 
		Otarget;

	BState const * target;

	bool 
		check_target, 
		prj_all;
	
	/* ---- Constructors/input ---- */
	void read_net_from_file(string fn)
	{
		try
		{
			filename = fn;

			ifstream orig;
			orig.open(filename.c_str(), ifstream::in);
			if(!orig.is_open()) throw runtime_error((string("cannot read from input file ") + filename).c_str());
			else cout << "Input file " << filename << " successfully opened" << endl;

			stringstream in;
			string line2;
			while (getline(orig, line2 = "")) 
			{
				const size_t comment_start = line2.find("#");
				in << ( comment_start == string::npos ? line2 : line2.substr(0, comment_start) ) << endl; 
			}
			orig.close();

			in >> BState::S >> BState::L;
			OState::S = BState::S, OState::L = BState::L;
			ushort s1, l1, s2, l2;
			trans_type ty;


			set<Transition> trans_set;;
			map<ushort, unsigned int> in_out_counter;
			unsigned int id = 0;
			string line;
			while (in)
			{
				getline (in,line);
				boost::tokenizer<boost::char_separator<char> > tok(line, boost::char_separator<char>(" "));
				boost::tokenizer<boost::char_separator<char> >::iterator i = tok.begin();

				//decode transition
				if(i == tok.end()) continue;
				s1 = boost::lexical_cast<shared_t>(*(i++));
				if(i == tok.end()) throw logic_error("invalid input line: " + line);
				l1 = boost::lexical_cast<local_t>(*(i++));
				if(i == tok.end()) throw logic_error("invalid input line: " + line);
			
				string sep = *(i++);
				if(sep == "->") 
					ty = thread_transition;
				else if(sep == "~>") 
					ty = transfer_transition;
				else if(sep == "+>") 
					ty = spawn_transition;
				else throw logic_error("invalid transition separator");
			
				if(i == tok.end()) throw logic_error("invalid input line: " + line);
				s2 = boost::lexical_cast<shared_t>(*(i++));
				if(i == tok.end()) throw logic_error("invalid input line: " + line);
				l2 = boost::lexical_cast<local_t>(*(i++));

				Thread_State 
					source(s1, l1), 
					target(s2, l2);

				if(!trans_set.insert(Transition(source,target,ty)).second)
				{
					cout << warning("multiple occurrences of transition ") << Transition(source,target,ty) << endl;
					continue;
				}

				//store transition
				if(s1 == s2)
					from_to_shared_counter[source.shared]++, 
					trans_list.push_back(Transition(source,target,ty,id,hor));
				else
					from_shared_counter[source.shared]++, 
					to_shared_counter[target.shared]++, 
					trans_list.push_back(Transition(source,target,ty,id,nonhor));

				switch(ty)
				{
				case thread_transition:
					adjacency_list[source].insert(target);
					inv_adjacency_list[target].insert(source);

					adjacency_list_tos[target.shared][source].insert(target);
					adjacency_list_tos2[target.shared].insert(Transition(source,target,thread_transition,id));
					break;
				case transfer_transition:
					transfer_adjacency_list[source].insert(target); 
					transfer_adjacency_list_tos[target.shared][source].insert(target);
					transfer_adjacency_list_froms[source.shared][source].insert(target);
					inv_transfer_adjacency_list[target].insert(source);

					adjacency_list_tos2[target.shared].insert(Transition(source,target,transfer_transition,id));
					break;
				case spawn_transition: 
					throw logic_error("spawn transition not supported (encode via thread transitions)");
				}

				++id;
			}
		}
		catch(exception&)
		{
			throw logic_error("could not parse input file (are the line endings correct?)");
		}
	}
	
	bool core_shared(const shared_t& s) const
	{
		if(prj_all || s == init.shared) return true; //otherwise e.g. 0 0 -> 1 1, 1 0 -> 0 1 would have no core state

		Net::shared_counter_map_t::const_iterator q = from_to_shared_counter.find(s);
		if(q != from_to_shared_counter.end() && q->second != 0) //for horizonal transitions, e.g. 2,2 -> 2,5
			return true;

		Net::shared_counter_map_t::const_iterator f = from_shared_counter.find(s), t =  to_shared_counter.find(s);
		return !(f != from_shared_counter.end() && t != to_shared_counter.end() && f->second == 1 && t->second == 1);
	}

	/* ---- Statistics ---- */
	struct net_stats_t
	{
		unsigned S,L,T;
		map<trans_type,unsigned> trans_type_counters;
		map<trans_dir_t,unsigned> trans_dir_counters;
		unsigned SCC_num; //total number of strongly connected components
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

		return ret;
	}

};

#endif