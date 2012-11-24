/******************************************************************************
  Synopsis		[Complement data structure.]

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

#ifndef LU_SET_H
#define LU_SET_H

#include "cmb_node.h"
#include "ostate.h"
#include "multiset_adapt.h"

#include <boost/thread.hpp>
#include <utility>
#include <vector>

/********************************************************
complement_set
********************************************************/
struct complement_set
{
	/* ---- Members and types ---- */	
	unsigned L; //vectors range over elements from 0...L-1
	unsigned K; //vectors range over at most K elements
	bool full_sat; //flag whether we traverse the precise complement or only states of the form l then l,l then l,l,l etc.
	us_cmb_node_p_t u_nodes; //pointer to nodes of the upper set
	us_cmb_node_p_t m_nodes; //pointer to nodes that are neither in the upper nor in the lower set, temporary nodes
	us_cmb_node_p_t l_nodes; //pointer to nodes of the lower set
	
	/* ---- Destructor ---- */
	~complement_set();
	
	/* ---- Constructors ---- */
	complement_set(unsigned l, unsigned k, bool f);
	complement_set(complement_set&&);

	/* ---- Manipulation ---- */
	bool insert(cmb_node& c);
	std::pair<bool,us_cmb_node_p_t> diff_insert(const cmb_node& c);

private:
	/* ---- Helpers ---- */
	void remove(cmb_node_p f);
	us_cmb_node_p_t diff_remove(cmb_node_p f);
	void try_expand_remove(cmb_node_p f);
	void diff_try_expand_remove(cmb_node_p f,us_cmb_node_p_t& u_nodes_diff);
};

/********************************************************
complement_vec
********************************************************/
struct complement_vec
{
	/* ---- Members and types ---- */	
	unsigned K;
	unsigned S;
	unsigned L;
	bool full_sat;
	std::vector<complement_set> luv;

	/* ---- Constructors ---- */
	complement_vec(unsigned k, unsigned s, unsigned l,bool full_sat);
	//complement_vec(complement_vec&&);
	std::pair<bool,us_cmb_node_p_t> diff_insert(shared_t s, const cmb_node& c);
	
	/* ---- Misc ---- */
	void clear();
	size_t lower_size();
	size_t upper_size();
	void print_lower_set(std::ostream& out) const;
	void print_upper_set(std::ostream& out) const;

	/* ---- Projection ---- */	
	unsigned project_and_insert(const OState& g);
};

/********************************************************
lowerset_vec
********************************************************/
struct lowerset_vec
{
	/* ---- Members and types ---- */	
	const unsigned K;
	bool full_sat;
	std::vector<us_cmb_node_p_t> lv;

	/* ---- Constructors ---- */
	lowerset_vec(unsigned k, unsigned s, bool full_sat = true);
	~lowerset_vec();

	/* ---- Constructors ---- */
	void clear();
	void swap(lowerset_vec&);
	
	/* ---- Projection ---- */
	unsigned project_and_insert(const OState&, shared_cmb_deque_t&, bool);
	size_t size() const;

};

#endif