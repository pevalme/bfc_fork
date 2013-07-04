/******************************************************************************
  Synopsis		[State object for the modified Karp-Miller procedure.]

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

#ifndef OSTATE_H
#define OSTATE_H

#include <queue>
#include <unordered_set>

#include "types.h"
#include "user_assert.h"
#include "unordered_priority_set.h"

/********************************************************
Support functions
********************************************************/
template<class it_T1, class it_T2, class oit_T> 
inline void merge_mult(it_T1 First1, it_T1 Last1, it_T2 First2, it_T2 Last2, const unsigned m, oit_T D){
	
	if(m == 0)
		return;
	
	for (; First1 != Last1 && First2 != Last2; ++D){
		if (*First2 < *First1){	// merge first
			for(unsigned i = 1; i < m; ++i, ++D) *D = *First2;
#ifdef ICPC
			*D = *First2;
#else
			*D = std::move(*First2);
#endif
			++First2;
		}else{	// merge second
#ifdef ICPC
			*D = *First1;
#else
			*D = std::move(*First1);
#endif
			++First1;
		}
	}

#ifdef ICPC
	D = std::copy(First1, Last1, D);
#else
	D = std::move(First1, Last1, D);	// copy any tail, replicate if neccessary 
#endif
	for(; First2 != Last2; ++D, ++First2){
		for(unsigned i = 1; i < m; ++i, ++D) *D = *First2;
#ifdef ICPC
		*D = *First2;
#else
		*D = std::move(*First2);
#endif
	}
}

template<class bit_T, class uit_T> 
inline bool leq(bit_T bfirst1, bit_T blast1, bit_T bfirst2, bit_T blast2, uit_T ufirst1, uit_T ulast1, uit_T ufirst2, uit_T ulast2){
	bool progress = true;
	while(progress){
		progress = false;

		if(bfirst1 != blast1 && (ufirst1 == ulast1 || *bfirst1 < *ufirst1)){
			if(bfirst2 != blast2 && (ufirst2 == ulast2 || *bfirst2 < *ufirst2)){ 
				if(*bfirst1 < *bfirst2)
					return false;
				else if(*bfirst2 < *bfirst1)
					++bfirst2, progress = true;
				else 
					++bfirst1, ++bfirst2, progress = true;
			}else if(ufirst2 != ulast2 && (bfirst2 == blast2 || *bfirst2 > *ufirst2)){
				if(*bfirst1 < *ufirst2)
					return false;
				else if(*ufirst2 < *bfirst1)
					++ufirst2, progress = true;
				else{
					while(bfirst1 != blast1 && *bfirst1 == *ufirst2) ++bfirst1;
					++ufirst2, progress = true;
				}
			}
		}else if(ufirst1 != ulast1 && (bfirst1 == blast1 || *ufirst1 < *bfirst1)){
			if(bfirst2 != blast2 && (ufirst2 == ulast2 || *bfirst2 < *ufirst2)){
				if(*ufirst1 < *bfirst2)
					return false;
				else if(*bfirst2 < *ufirst1)
					++bfirst2, progress = true;
				else{
					return false;
				}
			}else if(ufirst2 != ulast2 && (bfirst2 == blast2 || *bfirst2 > *ufirst2)){
				if(*ufirst1 < *ufirst2)
					return false;
				else if(*ufirst2 < *ufirst1)
					++ufirst2, progress = true;
				else
					++ufirst1, ++ufirst2, progress = true;
			}
		}
	}

	//note: via random testing these cases where found to be the only feasible ones
	invariant
		((bfirst1 == blast1 && bfirst2 == blast2 && ufirst1 == ulast1 && ufirst2 == ulast2)
		|| (bfirst1 == blast1 && bfirst2 == blast2 && ufirst1 == ulast1 && ufirst2 != ulast2) 
		|| (bfirst1 == blast1 && bfirst2 != blast2 && ufirst1 == ulast1 && ufirst2 == ulast2)
		|| (bfirst1 == blast1 && bfirst2 == blast2 && ufirst1 != ulast1 && ufirst2 == ulast2)
		|| (bfirst1 != blast1 && bfirst2 == blast2 && ufirst1 == ulast1 && ufirst2 == ulast2)
		|| (bfirst1 == blast1 && bfirst2 != blast2 && ufirst1 == ulast1 && ufirst2 != ulast2)
		|| (bfirst1 != blast1 && bfirst2 == blast2 && ufirst1 != ulast1 && ufirst2 == ulast2));

	if(bfirst1 == blast1 && ufirst1 == ulast1){
		return true;
	}else{
		return false;
	}
}

/********************************************************
OState
********************************************************/
struct OState;
struct OState_husher{ std::size_t operator()(const OState& p) const; };
struct OState_eqstr{ bool operator()(const OState& s1, const OState& s2) const; };
//typedef unordered_set<OState,OState_husher,OState_eqstr> Oreached_t;

typedef OState const * ostate_t;

struct ostate_t_husher{ std::size_t operator()(ostate_t const& p) const; };
struct ostate_t_eqstr{ bool operator()(ostate_t const& s1, ostate_t const& s2) const; };
typedef unordered_set<ostate_t,ostate_t_husher,ostate_t_eqstr> Oreached_t;

typedef std::pair<Oreached_t::const_iterator,int> Opriority_iterator;
struct Opriority_iterator_comparison_less{ bool operator() (Opriority_iterator& lhs, Opriority_iterator& rhs) const; };
struct Opriority_iterator_comparison_greater{ bool operator() (Opriority_iterator& lhs, Opriority_iterator& rhs) const; };

typedef unordered_priority_set<ostate_t> OState_priority_queue_t;

struct OState
{
	/* ---- Members and types ---- */	
	
	//hash-function sensitive data
	shared_t		shared;
	bounded_t		bounded_locals;
	unbounded_t		unbounded_locals;

	//hash-function insensitive data
	ostate_t		prede;
	ostate_t		accel; //state this was accelerated wrt.
	bool			tsucc; //successor over a transfer transition
	unsigned		depth; //exploration depth

	enum add_t { add_bounded, add_unbounded, add_none };

	/* ---- Constructors ---- */
	OState(shared_t = invalid_shared);
	template<class Iter> OState(shared_t, Iter, Iter);
	template<class Iter1,class Iter2> OState(shared_t, Iter1, Iter1, Iter2, Iter2);
	OState(string);
	static OState get_rand(unsigned, unsigned = 0, bool = false);
	Oreached_t resolve_omegas(size_t) const;

	/* ---- Order operators ---- */
	bool operator == (const OState& r) const;
	bool operator <= (const OState& r) const;

	/* ---- Output operators ---- */
	std::ostream& operator << (std::ostream& out) const;
	std::string id_str() const;
	std::string str_latex() const;	
	std::ostream& mindot(std::ostream& out, bool = false, string = "plaintext") const;

	/* ---- Misc ---- */
	void swap(OState& other);
	size_t size() const;
	bool consistent() const;
};

template<class Iter> OState::OState(shared_t s, Iter first, Iter last)
	: shared(s), bounded_locals(first,last), unbounded_locals(), accel(nullptr), prede(nullptr), tsucc(false), depth(0)
{
}

template<class Iter1,class Iter2> OState::OState(shared_t s, Iter1 first1, Iter1 last1, Iter2 first2, Iter2 last2)
	: shared(s), bounded_locals(first1,last1), unbounded_locals(first2,last2), accel(nullptr), prede(nullptr), tsucc(false), depth(0)
{
}

/* ---- Helper for output operator ---- */
std::ostream& operator << (std::ostream& out, const OState& t);

#endif