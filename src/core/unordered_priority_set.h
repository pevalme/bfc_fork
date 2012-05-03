/******************************************************************************
  Synopsis		[Unordered priority data struture.]

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

#ifndef UNORDERED_PRIOROTY_SET_H
#define UNORDERED_PRIOROTY_SET_H

#include <vector>

using namespace std;

template<class Kty>
struct unordered_priority_set
{
	typedef unordered_priority_set<Kty>						Myt;

	typedef Kty												key_type;
	typedef unordered_set<key_type>							unordered_key_types;
	typedef vector<unordered_key_types>						v_us_t;
	typedef typename unordered_key_types::const_iterator	bucket_const_iterator;
	typedef pair<key_type, size_t>							keyprio_type;

	enum order_t{
		less,
		greater,
		random
	};

	v_us_t					v_us;
	set<size_t>				non_empty;
	order_t					order;
	bucket_const_iterator	last_top;

	unordered_priority_set(order_t o = less): v_us(1), non_empty(), order(o), last_top(v_us[0].end())
	{
	}

	//return true if the element did not already exist, and otherwise false
	bool push(keyprio_type kp)
	{
		if(order == random) kp.second = 0;
		
		if(v_us.size() <= kp.second)
			v_us.resize(kp.second + 1);

		return non_empty.insert(kp.second), v_us[kp.second].insert(kp.first).second;
	}

	//return true if element was erased, and otherwise false
	bool erase(keyprio_type kp)
	{
		if(order == random) kp.second = 0;

		size_t erased = v_us[kp.second].erase(kp.first);
		invariant(erased == 1);
		if(v_us[kp.second].empty())
			non_empty.erase(kp.second);
		
		return erased == 1;
	}

	bool empty() const
	{
		return non_empty.empty();
	}

	bool contains(keyprio_type kp) const
	{
		if(order == random) kp.second = 0;
		
		if(non_empty.find(kp.second) == non_empty.end())
			return false;
		
		return v_us[kp.second].find(kp.first) != v_us[kp.second].end();
	}

	bucket_const_iterator find(keyprio_type kp) const
	{
		if(order == random) kp.second = 0;
		invariant(contains(kp));
		
		return v_us[kp.second].find(kp.first);
	}

	void swap(Myt& r)
	{
		if (this != &r)
			this->v_us.swap(r.v_us), this->non_empty.swap(r.non_empty), std::swap(this->order, r.order);
	}

	unsigned cur_max_prio()
	{
		return *non_empty.rbegin();
	}

	unsigned cur_min_prio()
	{
		return *non_empty.begin();
	}

	bool try_get(key_type& r)
	{
		if(empty()) return false;
		r = top().first, pop();
		return true;
	}

	keyprio_type top()
	{
		invariant(!empty());
		
		if(order == random){
			if(last_top != v_us[0].end())
				return make_pair(*last_top,0);

			invariant(last_top == v_us[0].end());
			invariant(!v_us[0].empty());
			int r = rand()%v_us[0].size();
			last_top = v_us[0].begin();
			while(r--)
				++last_top;
			invariant(last_top != v_us[0].end());

			return make_pair(*last_top,0);
		}

		size_t prio;
		switch(order){
		case less: prio = *non_empty.begin(); break;
		case greater: prio = *non_empty.rbegin(); break;
		}
		invariant(!v_us[prio].empty());

		return make_pair(*v_us[prio].begin(),prio);
	}

	void pop()
	{
		if(order == random){
			invariant(last_top != v_us[0].end());
			v_us[0].erase(last_top);
			last_top = v_us[0].end();
			if(v_us[0].empty())
				non_empty.erase(0);

			return;
		}

		size_t prio;
		switch(order){
		case less: prio = *non_empty.begin(); break;
		case greater: prio = *non_empty.rbegin(); break;
		}
		invariant(!v_us[prio].empty());
		v_us[prio].erase(v_us[prio].begin());

		if(v_us[prio].empty())
			non_empty.erase(prio);

		return;
	}

	size_t size() const
	{
		size_t r = 0;
		foreach(size_t s, non_empty)
			r += v_us[s].size();
		return r;
	}

};

#endif