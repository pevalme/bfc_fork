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

#include "types.h"
#include "tstate.h"
#include "trans.h"
#include "ostate.h"
#include "bstate.h"

#include <map>

/********************************************************
Net
********************************************************/
struct Net{

	/* ---- Types ---- */	
	enum net_format_t {MIST,TIKZ,TTS,LOLA,TINA,CLASSIFY}; //CLASSIFY is no net type

	typedef std::map<Thread_State, std::map<Thread_State, transfers_t> > adj_t; //permit one transfer per sl->s'l' transition only
	typedef std::map<shared_t, unsigned> shared_counter_map_t;
	typedef std::map<unsigned, bool> locals_boolvec_t;
	typedef std::map<local_t,std::set<shared_t> > ls_m_t;
	typedef std::map<local_t,ls_m_t> lls_map_t;
	typedef std::map<shared_t, std::set<Transition> > s_adjs_t;

	//backward transitions
	typedef std::vector<std::vector<Transition> > vv_adjs_t;
	typedef std::vector<std::vector<std::vector<Transition> > > vvl_adjs_t;

	typedef std::vector<local_t> local_perm_t;

	struct stats_t
	{
		unsigned S,L,T;
#ifdef USE_STRONG_COMPONENT
		unsigned SCC_num; //total number of strongly connected components
#endif
		unsigned discond; //number of thread states that have no incoming or outgoing edge
		unsigned max_indegree;
		unsigned max_outdegree;
		unsigned max_degree;
		unsigned core_shared_states;
	};


	/* ---- Members ---- */	
	/* Dimension */	
	shared_t S_input, S; //shared states before and after rewriting
	local_t L_input, L; //local states before and after rewriting

	/* Initial and target state */	
	OState init;
	BState target;

	/* Transition relation */	
	string filename, targetname, initname;
	adj_t adjacency_list; //, adjacency_list_inv;

	/* Misc */
public: //TODO: remove
	mutable bool net_changed;
	
	/* Output */	
	ostream_sync reduce_log; //reduction

	/* ---- Constructors ---- */
	Net(const Net &);
	Net();
	//Net(shared_t, local_t, OState, BState, adj_t, adj_t, bool);
	Net(shared_t, local_t, OState, BState, adj_t, bool);
	Net(string, string, string, bool);
	
	/* ---- Access ---- */	
	s_adjs_t get_backward_adj() const;
	std::pair<vv_adjs_t,vvl_adjs_t> get_backward_adj_2() const;

	adj_t& adjacency_list_inv() const;
	vector<bool> core_shared;

	stats_t get_stats(bool sccnetstats) const;

	/* ---- Reduction ---- */	
	void reduce(bool,bool);
	std::vector<bool> get_core_shared(bool = false, bool = false) const;

	/* ---- Misc ---- */	
	void swap(Net&);
};

ostream& operator << (ostream&, const Net&);

#endif