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

#ifndef U_SET_DCLS_H
#define U_SET_DCLS_H

#include "vstate.h"
#include "bstate.h"
#include "multiset_adapt.h"

/********************************************************
Antichain data-structure

note: this class must not be copy-constructed!!
********************************************************/
struct antichain_t
{
	
	/* ---- Members and types ---- */	
	typedef std::set<bstate_t>	s_scpc_t;
	
	struct CG_t
	{
		us_VState_t	nodes;
		VState * bot,* top;
		
		CG_t();
		CG_t(CG_t&&);
		
		~CG_t();

		void clear();
		void swap(CG_t&);
		
		CG_t& operator=(const CG_t& rhs);
	};

	struct insert_t
	{
		po_rel_t	case_type;
		bstate_t	new_el; //pointer to element from M
		s_scpc_t	exist_els; //pointers to elements from states (neq_ge) or suspended_states (neq_le)
		
		insert_t(po_rel_t c, bstate_t i, s_scpc_t il);
		insert_t(po_rel_t c, bstate_t i, bstate_t il);
		insert_t();
	};

	enum order_t{less_equal,greater_equal};

	CG_t				G; //support initial nodes/containment graphs for LE and GE operations
	Breached_p_t		M; //set of pointers to minimal elements
	bool				ownership; //indicates whether the minimal elements shall be deleted upon destruction

	/* ---- Constructors ---- */
	antichain_t(bool = false);
	antichain_t(antichain_t&&);

	/* ---- Destructor ---- */
	~antichain_t();

	/* ---- Order operators ---- */
	antichain_t& operator=(const antichain_t &rhs);

	/* ---- Manipulation ---- */
	insert_t case_insert(bstate_t s);
	insert_t max_case_insert(bstate_t s);

	void erase(BState const * x); //description: clean look-up table and M set
	bool manages(BState const * s) const;
	void clear();

	/* ---- Misc ---- */
	s_scpc_t LGE(bstate_t p, const order_t order);
	
	/* ---- Direct access ---- */
	po_rel_t relation(BState const * s);
	void insert_incomparable(BState const * s);
	s_scpc_t insert_neq_le(BState const * s);
	std::pair<bstate_t, bool> insert(BState const * s);
	std::pair<bstate_t, bool> max_insert(BState const * s);
	const Breached_p_t& M_cref() const;

private:
	//description: update look-up table and M set
	void integrate(BState const * x);
	VState decompose(bstate_t p) const;
	
};


/* ---- Global types ---- */	
struct vec_antichain_t
{
	/* ---- Types ---- */	
	typedef antichain_t set_t;

	/* ---- Members ---- */	
	std::vector<set_t> uv;

	/* ---- Constructors ---- */
	vec_antichain_t(bool = false);
	
	/* ---- Destructor ---- */
	~vec_antichain_t();

	/* ---- Misc ---- */
	size_t size() const;
	size_t graph_size() const;

	/* ---- Access ---- */
	antichain_t::insert_t case_insert(bstate_t s);
	std::pair<bstate_t, bool> insert(BState const * s, bool safe_free = false);
	antichain_t::insert_t max_case_insert(bstate_t s);
	std::pair<bstate_t, bool> max_insert(BState const * s, bool safe_free = false);
	std::set<bstate_t> LGE(BState const * s, antichain_t::order_t order);

	void insert_incomparable(BState const * s);
	antichain_t::s_scpc_t insert_neq_le(BState const * s);
	void erase(BState const * s);
	bool manages(BState const * s);
	const Breached_p_t& M_cref(shared_t shared) const;
	po_rel_t relation(BState const * s);
};

#endif
