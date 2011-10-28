/******************************************************************************
  Synopsis		[Antichain data structure.]

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

#ifndef U_SET_H
#define U_SET_H

template<class _Ty1>
struct second_greater : public binary_function<_Ty1, _Ty1, bool>{
	bool operator()(const _Ty1& _Left, const _Ty1& _Right) const{
		return (_Left.second > _Right.second);
	}
};

template <class T>
set<T>& SetSet_operator_PlusEq(set<T>& l, const set<T>& r){
	foreach(const T& e, r) l.insert(e);
	return l;
}

template <class T>
set<T>& operator += (set<T>& l, const set<T>& r){

	typename set<T>::iterator _First1 = l.begin(), _Last1 = l.end(), _First2 = r.begin(), _Last2 = r.end();

	for (; _First1 != _Last1 && _First2 != _Last2; ){
		if (*_First1 < *_First2) 
			++_First1;
		else if (*_First2 < *_First1) {
#ifdef WIN32
			l.emplace_hint(--set<T>::iterator(_First1),*_First2++);
#else
			l.insert(*_First2++);
#endif
			//++_First2;
		}
		else 
			++_First1, ++_First2;
	}

	for(;_First2 != _Last2;){
		if(l.empty())
#ifdef WIN32
			l.emplace_hint(l.begin(),*_First2++);
#else
			l.insert(*_First2++);
#endif
		else
#ifdef WIN32
			l.emplace_hint(--l.end(),*_First2++);
#else
			l.insert(*_First2++);
#endif
	}

	return l;
}

template <class T>
set<T>& operator *= (set<T>& l, const set<T>& r){
	typename set<T>::iterator _First1 = l.begin(), _Last1 = l.end(), _First2 = r.begin(), _Last2 = r.end();

	for (; _First1 != _Last1 && _First2 != _Last2; )
		if (*_First1 < *_First2) l.erase(_First1++);
		else if (*_First2 < *_First1) ++_First2;
		else ++_First1, ++_First2;

	for (; _First1 != _Last1;)
		l.erase(_First1++);
	
	return l;
}

template <class T>
set<T>& operator -= (set<T>& l, const set<T>& r){
	typename set<T>::iterator _First1 = l.begin(), _Last1 = l.end(), _First2 = r.begin(), _Last2 = r.end();

	for (; _First1 != _Last1 && _First2 != _Last2; )
		if (*_First1 < *_First2) ++_First1++;
		else if (*_First2 < *_First1) ++_First2;
		else l.erase(_First1++), ++_First2;

	return l;
}

bool operator < (const Breached_t::iterator& l, const Breached_t::iterator& r){ return &*l < &*r; }

//note: u_set_vm currently ignores the shared state --> must therefore be used in a vector
struct u_set_vm{
	//note: unordered_set::insert can invalidate iterators, but only if the insert causes the load factor to be greater to or equal to the maximum load factor. Pointers and references to elements are never invalidated.
	//hence, use iterators only in-between insertions
	
	typedef set<bstate_t>			s_scpc_t; //was: si_t //list might be better
	typedef vector<s_scpc_t>	vsi_t;
	typedef vector<vsi_t>		vvsi_t;

	u_set_vm(bool reserve = true): states(), m(reserve?BState::L:0) { debug_assert(BState::L != 0); } //do not allocate with argument -1

	struct insert_t{
		po_rel_t	case_type;
		bstate_t		new_el; //pointer to element from states
		s_scpc_t	exist_els; //pointers to elements from states (neq_ge) or suspended_states (neq_le)
		
		insert_t(po_rel_t c, bstate_t i, s_scpc_t il): case_type(c), new_el(i), exist_els(il) {}
		insert_t(po_rel_t c, bstate_t i, bstate_t il): case_type(c), new_el(i), exist_els() { exist_els.insert(il); }
	};

	insert_t case_insert(bstate_t s){
		//note: s must be globally accessible, as it is only referenced in states
		assert(s->consistent());

		Breached_p_t::iterator f = states.find(s);
		if(f != states.end()) { 
			//case: eq -- s is exactly contained in states ("match")
			return insert_t(eq,nullptr,*f); //note: return f, not s here!
		}

		s_scpc_t le = LGE(s,less_equal);
		if(!le.empty()){ 
			//case: neq_ge -- s is already contained in us(states), though not exactly ("overcut")
			return insert_t(neq_ge,nullptr,le);
		}
		
		s_scpc_t lg = LGE(s,greater_equal);
		//clean look-up table and states set (non-minimal elements are removed)
		foreach(bstate_t const& x, lg) erase(x);
		
		//update look-up table and states set
		integrate(s);

		//cases: neq_le, neq_nge_nle
		return insert_t((lg.empty()?(neq_nge_nle):(neq_le)), s, lg);
	}

	pair<bstate_t, bool> insert(BState const * s, bool safe_free = false){ //note: the flag safe_free indicates whether s should be freed if it is not accessed by this object anymore
		insert_t r(case_insert(s));
		
		if(safe_free && (r.case_type == neq_nge_nle || r.case_type == neq_le))
			delete s;
		
		return make_pair(r.new_el,r.case_type == neq_nge_nle || r.case_type == neq_le);
	}
	
	//description: clean look-up table and states set
	void erase(BState const * s, bool free = false){
		debug_assert(states.find(s) != states.end());
		
		states.erase(s);
		for(local_t l = 0; l < BState::L; ++l)
			m[l][s->bounded_locals.count(l)].erase(s);

		if(free) delete s;
	}

	void clear(){
		m = vvsi_t(BState::L);
		states.clear();
	}

	bool manages(BState const * s){
		//note: this will check whether a state y in states exsits with *y = *s
		return states.find(s) != states.end();
	}

	bool manages_exact(BState const * s){
		//note: this will check whether a state y in states exsits with y = s
		Breached_p_t::iterator f = states.find(s);
		if(f == states.end()) return false;

		return *f == s;
	}

	const Breached_p_t& states_cref() const{
		return states;
	}

private:
	enum order_t{less_equal,greater_equal};

	//description: update look-up table and states set
	void integrate(BState const * s){
		for(local_t l = 0; l < BState::L; ++l){
			if(m[l].size() <= s->bounded_locals.count(l)) 
				m[l].resize(s->bounded_locals.count(l) + 1);
			m[l][s->bounded_locals.count(l)].insert(s);
		}
		states.insert(s);
	}

	s_scpc_t LGE(bstate_t p, const order_t order) {
		typedef pair<local_t,unsigned> pl_t;
		typedef priority_queue<pl_t,vector<pl_t>,second_greater<pl_t> > osize_t; //note: this set<pair<l, prio> > might be better here
		
		osize_t osize;
		static local_t l;
		static unsigned n;
		for(l = 0; l < BState::L; ++l){
			static local_t L; L = 0;
			
			for(n = min(p->bounded_locals.count(l), (order == less_equal)?m[l].size()-1:-1u); n < m[l].size(); (order == less_equal)?--n:++n)
				L += m[l][n].size();
			
			if(L != 0) osize.push(make_pair(l,L));
			else return s_scpc_t(); //no other vector v exists with v[l]<=/>=p->bounded_locals.count(l)
		}

		static s_scpc_t isec; isec.clear();
		while(!osize.empty()){
			static s_scpc_t uni;
			l = osize.top().first; osize.pop();
			
			for(n = min(p->bounded_locals.count(l), (order == less_equal)?m[l].size()-1:-1u); n < m[l].size(); (order == less_equal)?--n:++n)
				uni += m[l][n];
			
			if(isec.empty()) isec.swap(uni);
			else{
				isec *= uni, uni.clear();
				if(isec.empty()) break;
			}
		}

		return isec;
	}

	protected:

	//protected data members
	Breached_p_t states; //store of minimal elements
	vvsi_t m; //support map for LE and GE operations
};

struct u_vec_vm{
	vector<u_set_vm> uv;
	u_vec_vm(bool prj_all = true): uv(0,u_set_vm(false)) { //note: luv(s, complement_set(l,k)) in the initializer list does not work, since all shared states will then get the same nodes
		for(unsigned i = 0; i < BState::S; ++i){
			if(prj_all || net.core_shared(i))
				uv.push_back(u_set_vm(true));
			else
				uv.push_back(u_set_vm(false));
		}
	}
};

struct u_map{
	map<shared_t, u_set_vm> uv; //performance: Each u_set_vm currently allocates a vector of size(BState::L). If only few local states are used, one could use an unordered_set or a map instead to save space. 
	u_map(): uv() {}
};

#endif