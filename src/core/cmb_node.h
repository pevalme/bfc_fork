/******************************************************************************
  Synopsis		[Node object for complement data structure.]

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

#ifndef CMB_NODE_H
#define CMB_NODE_H

#include <cstring> //size_t
#include <unordered_set>
#include <queue>
#include <list>
#include "types.h"

/********************************************************
cmb_node

note: allocated memory for lt an rc is not deallocated
********************************************************/
struct cmb_node;
typedef cmb_node const* cmb_node_p;

struct cmb_node__husher{ std::size_t operator()(const cmb_node_p&) const; };
struct cmb_node__eqstr{ bool operator()(const cmb_node_p&, const cmb_node_p&) const; };
typedef std::unordered_set<cmb_node_p,cmb_node__husher,cmb_node__eqstr> us_cmb_node_p_t;

#include <boost/thread.hpp>

struct mu_list
{
	typedef std::list<std::pair<shared_t, cmb_node_p> > lst_t;
	
	lst_t*			lst;
	boost::mutex	mtx;
	
	mu_list(): lst(new lst_t())
	{
	}

	~mu_list()
	{
		delete lst;
	}
};

typedef mu_list shared_cmb_deque_t;

struct cmb_node{
	/* ---- Members and types ---- */
	typedef std::vector<local_t> cmb_t;
	
	cmb_t					c;  //combination
	std::list<cmb_node_p>*	lt; //pointer to pointers to all super sets
	unsigned*				rc; //pointer to ref counter

	/* ---- Constructors ---- */
	cmb_node(const cmb_node& r, bool alloc = true);
	cmb_node(size_t count = 0);
	cmb_node(size_t count, local_t val);
	template<class _Iter> cmb_node(_Iter b, _Iter e, unsigned rc_val, cmb_node* lt_node);

	/* ---- Destructor ---- */
	~cmb_node();
	
	/* ---- Operators ---- */
	cmb_node& operator=(const cmb_node& r);
	
	/* ---- Misc ---- */
	std::ostream& operator << (std::ostream& out) const;

};

template<class _Iter>
cmb_node::cmb_node(_Iter b, _Iter e, unsigned rc_val, cmb_node* lt_node)
	:c(b,e), rc(new unsigned(rc_val)), lt(new std::list<cmb_node_p>(lt_node==NULL?0:1,lt_node))
{
}

/* ---- Helper for output operator ---- */
std::ostream& operator << (std::ostream& out, const cmb_node& t);

#endif