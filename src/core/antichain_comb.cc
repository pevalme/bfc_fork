#include "antichain_comb.h"

#include "net.h"
#include "vstate.h"
#include "bstate.h"

#include <stack>

using namespace std;

antichain_comb_t::antichain_comb_t(bool ownership_): ownership(ownership_), V(BState::S)
{
}

//antichain_comb_t::antichain_comb_t(antichain_comb_t&& other)
//{
//	M.swap(other.M);
//	V.swap(other.V);
//	ownership = other.ownership, other.ownership = false;
//}

antichain_comb_t::~antichain_comb_t()
{
	if(!ownership)
		return;
	
	for(auto& m : M)
		delete m;
}

antichain_comb_t& antichain_comb_t::operator=(const antichain_comb_t &rhs)
{
	M = rhs.M;
	V = rhs.V;
	return *this;
}

insert_t antichain_comb_t::case_insert(bstate_t s)
{
	//note: only checks for smaller or equal elements (they do not harm since we deal with upward-closed sets), not for larger (expensive and scarcely needed in practice)
	//note: s must be globally accessible, as it is only referenced in M
	invariant(s->consistent());
	invariant(s->bounded_locals.size() != 0);

	Breached_p_t::iterator f = M.find(s);
	if(f != M.end()) 
		return insert_t(eq,nullptr,*f);

	s_scpc_t le = LGE(s,less_equal);
	if(!le.empty())
		return insert_t(neq_ge,nullptr,le);

	M.insert(s), V[s->shared].insert(cmb_node::cmb_t(s->bounded_locals.begin(),s->bounded_locals.end())); // add elements to V

	return insert_t(neq_le__or__neq_nge_nle, s, s_scpc_t());
}

po_rel_t antichain_comb_t::relation(BState const * s, bool thorough)
{
	if(thorough)
	{
		s_scpc_t le = LGE(s,less_equal,false);
		if(!le.empty()) return neq_ge;
	}

	Breached_p_t::iterator f = M.find(s);
	if(f != M.end()) return eq;

	if(!thorough)
	{
		s_scpc_t le = LGE(s,less_equal,false);
		if(!le.empty()) return neq_ge;
	}

	return neq_le__or__neq_nge_nle;
}

void antichain_comb_t::insert_incomparable(BState const * s)
{
	invariant(M.find(s) == M.end());
	invariant(LGE(s,less_equal,false).empty());
	M.insert(s), V[s->shared].insert(cmb_node::cmb_t(s->bounded_locals.begin(),s->bounded_locals.end())); // add elements to V
}

pair<bstate_t, bool> antichain_comb_t::insert(BState const * s)
{
	insert_t r(case_insert(s));

	return make_pair(r.new_el,r.case_type == neq_le__or__neq_nge_nle);
}

//description: clean look-up table and M set
void antichain_comb_t::erase(BState const * s)
{
	invariant(manages(s));

	M.erase(s), V[s->shared].erase(V[s->shared].find(cmb_node::cmb_t(s->bounded_locals.begin(),s->bounded_locals.end()))); // add elements to V
}

void antichain_comb_t::clear()
{
	M.clear();
	V.clear();
}

bool antichain_comb_t::manages(BState const * s) const
{
	//note: this will check whether a state y in M exsits with *y = *s
	return M.find(s) != M.end();
}

const Breached_p_t& antichain_comb_t::M_cref() const
{
	return M;
}

#include "combination.hpp"
s_scpc_t antichain_comb_t::LGE(bstate_t g, const order_t order, bool get_all)
{
	//precondition(M.find(g) == M.end());
	precondition(order == less_equal);
	precondition(g->size());
	
	s_scpc_t R;

	static vector<cmb_node::cmb_t> prj; //note: these can be reused for all calls
	static vector<local_t> h; 
	static BState b;
	unsigned n;

	n = g->bounded_locals.size();
	b.shared = g->shared;

	if(h.size() < n) h.resize(n, invalid_local);
	copy(g->bounded_locals.begin(), g->bounded_locals.end(),h.begin());

	for(unsigned i = prj.size() + 1; i < n; ++i) //for a given k, this loop is executed only once
		prj.push_back(cmb_node::cmb_t(i,invalid_local)); 

	for(unsigned r = 1; r < n ; ++r){
		do {
			cmb_node::cmb_t& c = prj[r-1];
			invariant(r < h.size());
			invariant(is_sorted(h.begin(), h.begin() + r));

			copy(h.begin(), h.begin() + r, c.begin());
			auto f = V[g->shared].find(c);
			if(f != V[g->shared].end())
			{
				b.bounded_locals.clear(), b.bounded_locals.insert(c.begin(),c.end());
				invariant(M.find(&b) != M.end());
				R.insert(*(M.find(&b)));
				if(!get_all) { r=n; break; }
			}
		} while (boost::next_combination(h.begin(), h.begin() + r, h.begin() + n));
	}

	return R;
}
