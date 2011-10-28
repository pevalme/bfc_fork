/******************************************************************************
  Synopsis		[Node objects for antichain data structures.]

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

#ifndef VSTATE_H
#define VSTATE_H

#include "bstate.h"

#include <algorithm>

/********************************************************
vector<local_t> order operators
********************************************************/
po_rel_t cmp__leq_eq(const std::vector<local_t>& l, const std::vector<local_t>& r);
bool operator <= (const std::vector<local_t>& l, const std::vector<local_t>& r);
inline bool operator != (const std::vector<local_t>& l, const std::vector<local_t>& r);
inline bool operator == (const std::vector<local_t>& l, const std::vector<local_t>& r);

/********************************************************
VState
********************************************************/
struct VState;
struct VState_husher{ vec_husher vh; std::size_t operator()(const VState& v) const; };
struct VState_eqstr{ vec_eqstr ve; bool operator()(const VState& v1, const VState& v2) const; };
typedef unordered_set<VState,VState_husher,VState_eqstr> us_VState_t;

struct VState
{
	/* ---- Members and types ---- */
	enum type_t{bot,def,top};

	struct blocking_t
	{
		unordered_set<bstate_t>			suc; 
		unordered_set<VState const*>	blocked_by__set;
		unordered_set<VState const*>	blocks__set;
	};
	
	type_t					type; //hash-function sensitive data
	std::vector<local_t>	bounded_locals; //hash-function sensitive data
	blocking_t*				bl; //hash-function insensitive data

	/* ---- Constructors ---- */
	VState(VState&&);
	VState(type_t t = def);
	template<class Iter> VState(Iter first, Iter last, size_t reserve);
	template<class Iter> VState(Iter first, Iter last);

	/* ---- Destructor ---- */
	~VState();

	/* ---- Order operators ---- */
	bool operator == (const VState& r) const;
	bool operator <= (const VState& r) const;

	/* ---- Containment graph ---- */
	std::pair<VState const *, bool> enqueue(VState const *bot, VState const *top) const;
	void dequeue() const;

	/* ---- Misc ---- */
	bool consistent() const;
};

template<class Iter>
VState::VState(Iter first, Iter last, size_t reserve): type(def), bl(new blocking_t)
{
	bounded_locals.reserve(reserve);
	for( ;first != last; ++first)
		bounded_locals.push_back(*first);
	debug_assert(std::is_sorted(bounded_locals.begin(),bounded_locals.end()));
}

template<class Iter>
VState::VState(Iter first, Iter last): type(def), bl(new blocking_t)
{
	Iter prev_first(first);
	for( ;first != last; ++first)
		if(*prev_first != *first)
			bounded_locals.push_back(*prev_first), prev_first = first;
	if(prev_first != last) 
		bounded_locals.push_back(*prev_first);
	debug_assert(std::is_sorted(bounded_locals.begin(),bounded_locals.end()));
}

#endif
