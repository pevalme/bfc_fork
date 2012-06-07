//#define USE_SETS

/******************************************************************************
  Synopsis		[State object for greedy algorithm.]

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

#ifndef MSET_STATE_H
#define MSET_STATE_H

#include "types.h"
#include <queue>
#include <unordered_set>
#include "ostate.h"

struct vec_antichain_t;
struct antichain_t;

/********************************************************
BState
********************************************************/
struct BState;
typedef BState const* bstate_t;
typedef BState* bstate_nc_t;

struct husher{ std::size_t operator()(const BState& p) const; };
struct eqstr{ bool operator()(const BState& s1, const BState& s2) const; };
typedef unordered_set<BState,husher,eqstr> Breached_t;

struct husher_p{ std::size_t operator()(bstate_t const& p) const; };
struct eqstr_p{ bool operator()(bstate_t const& s1, bstate_t const& s2) const; };
typedef unordered_set<bstate_t,husher_p,eqstr_p> Breached_p_t;

struct vec_husher{ std::size_t operator()(const std::vector<local_t>& v) const; };
struct vec_eqstr{ bool operator()(const std::vector<local_t>& v1, const std::vector<local_t>& v2) const; };

/* ---- Priority queue---- */
typedef std::pair<Breached_t::const_iterator,int> Bpriority_iterator;
struct priority_iterator_comparison{ bool operator() (Bpriority_iterator& lhs, Bpriority_iterator& rhs) const; };
typedef std::priority_queue<Bpriority_iterator,std::vector<Bpriority_iterator>,priority_iterator_comparison> work_priority_queue_t;

#define INIT_BUCKETS (0)

enum po_rel_t
{
	neq_le = 0,		//covered by
	eq = 1,			//equal
	neq_ge = 2,		//covers
	neq_nge_nle	= 3,//incomparable

	//combinations
	neq_ge__or__neq_nge_nle = 4,
	neq_le__or__neq_nge_nle = 5,
	neq_ge__or__neq_le = 6
};

struct BState{
	/* ---- Members and types ---- */	
	enum status_t{
		unset,				//not set (default)
		/* ---- not in work list ---- */
		//not yet processed
		blocked_pending, //this state is not in the work list since a le_neq state exists (it was no processed, i.e. pending, when the le_neq state was discovered)
		//processed		
		blocked_processed, //this state is not in the work list since a le_neq state exists (it was already processed when the le_neq state was discovered)
		processed,			//this state was already processed		
		/* ---- in work list ---- */
		pending				//this state is in the work list and waits for being processed
	};

	enum type_t{ bot, def, top, invalid };

	struct neighborhood_t
	{
#ifdef USE_SETS
		typedef set<bstate_t> pre_set_t;
		typedef set<bstate_t> suc_set_t;
#else
		typedef unordered_set<bstate_t> pre_set_t;
		typedef unordered_set<bstate_t> suc_set_t;
#endif

		bool					ini; //marks the source state (previously stored via src==this which is not possible anymore if the source is moved to a different "source tree")
		bstate_t				src; //the source
		pre_set_t				pre; //the predecessor
		suc_set_t				suc; //the successors

		status_t				status; //this should go somewhere else
		bool					sleeping; //this should go somewhere else

		unsigned				depth,gdepth; //depth in local tree/global depth

#ifdef USE_SETS
		neighborhood_t(bstate_t = nullptr, pre_set_t = pre_set_t(), suc_set_t = suc_set_t(), status_t m = unset, bool sleeps = false);
#else
		neighborhood_t(bstate_t = nullptr, pre_set_t = pre_set_t(INIT_BUCKETS), suc_set_t = suc_set_t(INIT_BUCKETS), status_t m = unset, bool sleeps = false);
#endif
	};

	struct blocking_t{
#ifdef USE_SETS
		typedef set<bstate_t> blocking_set_t;
#else
		typedef unordered_set<bstate_t> blocking_set_t;
#endif

		blocking_set_t	blocked_by;	//states that block this
		blocking_set_t	blocks;	//states this blocks

		blocking_t();
	};

	typedef vec_antichain_t vec_upperset_t;
	typedef antichain_t upperset_t;

	//hash-function sensitive data
	type_t				type;
	shared_t			shared;
	bounded_t			bounded_locals;

	//hash-function insensitive data
	neighborhood_t*				nb;
	blocking_t*					bl;
	vec_upperset_t*				us;
	unsigned					fl; //flags; for temporary usage

	static size_t L, S;

	/* ---- Destructor ---- */
	~BState();

	/* ---- Constructors ---- */
	BState(shared_t s = invalid_shared, bool alloc = false, type_t t = def);
	template<class Iter> BState(shared_t s, Iter first, Iter last, bool alloc = false, bstate_t src = nullptr, type_t t = def);
	BState(std::string str, bool alloc = false);
	void allocate();
	void deallocate();

	/* ---- Order operators ---- */
	bool operator == (const BState& r) const;
	bool operator <= (const BState& r) const;
	bool operator <= (const OState& r) const;

	/* ---- Output operators ---- */
	std::ostream& operator << (std::ostream& out) const;
	std::string id_str() const;
	std::string str_latex() const;

	/* ---- Misc ---- */
	size_t size() const;
	void swap(BState& other);
	enum check_t {partial_check, full_check};
	bool consistent(check_t c = partial_check) const;

	static BState get_rand(unsigned max_el, unsigned rep)
	{
		invariant(rep >= 0 && rep <= 100); //rep in 0...100

		BState r(rand()%S);
		while(max_el)
		{
			unsigned l = rand()%L;
			do r.bounded_locals.insert(l), --max_el;
			while(max_el && ((unsigned)rand()%101)<=rep);
		}
		invariant(r.consistent());
		return r;
	}

};

template<class Iter>
BState::BState(shared_t s, Iter first, Iter last, bool alloc, bstate_t src, type_t t)
	: type(t), shared(s), bounded_locals(first,last), nb(alloc?new neighborhood_t():nullptr), bl(alloc?new blocking_t():nullptr), us(nullptr), fl(0)
{
	if(alloc) nb->src = src;
}

/* ---- Helper for output operator ---- */
std::ostream& operator << (std::ostream& out, const BState& t);

/********************************************************
containment graph
********************************************************/
#include <set>
void dequeue(BState const* x);
bool enqueue(const std::set<bstate_t>& bots, const std::set<bstate_t>& tops, BState const* x);
bool enqueue(bstate_t& bot, const std::set<bstate_t>& tops, BState const* x);
std::pair<BState const *, bool> enqueue(BState const *bot, BState const *top, BState const* x);

#endif