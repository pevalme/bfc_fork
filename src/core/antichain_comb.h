/******************************************************************************
  Synopsis		[Wrapper for antichain data structure.]

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

#ifndef U_SET_COMB_H
#define U_SET_COMB_H

#include "vstate.h"
#include "bstate.h"
#include "multiset_adapt.h"
#include "cmb_node.h"

#include <vector>

/********************************************************
Antichain data-structure

note: this class must not be copy-constructed!!
********************************************************/
struct antichain_comb_t
{
	
	/* ---- Members and types ---- */	
	enum order_t{less_equal,greater_equal};

	typedef			container_hash<cmb_node::cmb_t>				hasher;
	typedef			std::unordered_set<cmb_node::cmb_t,hasher>	u_cmb_t;
	//typedef			std::vector<u_cmb_t>						vs_cmb_t;
	typedef			std::unordered_map<shared_t,u_cmb_t>			vs_cmb_t;

	
	Breached_p_t	M; //set of pointers to minimal elements
	vs_cmb_t		V;
	bool			ownership; //indicates whether the minimal elements shall be deleted upon destruction

	/* ---- Constructors ---- */
	antichain_comb_t(bool = false);
	//antichain_comb_t(antichain_comb_t&&);

	/* ---- Destructor ---- */
	~antichain_comb_t();

	/* ---- Order operators ---- */
	antichain_comb_t& operator=(const antichain_comb_t &rhs);

	/* ---- Manipulation ---- */
	insert_t case_insert(bstate_t s);

	void erase(BState const * x); //description: clean look-up table and M set
	bool manages(BState const * s) const;
	void clear();

	/* ---- Misc ---- */
	s_scpc_t LGE(bstate_t p, const order_t order, bool = true);
	
	/* ---- Direct access ---- */
	po_rel_t relation(BState const * s,bool = false);
	void insert_incomparable(BState const * s);
	std::pair<bstate_t, bool> insert(BState const * s);
	const Breached_p_t& M_cref() const;
};
#endif
