#ifndef MINBW_H
#define MINBW_H

#include "types.h"
#include "net.h"
#include "ostate.h"
#include "bstate.h"
#include "complement.h"

#include <boost/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>
#include "antichain_comb.h"

const unsigned 
	pruning = 0,
	//discarding = 1,
	//restarting = 2
	restarting = 1, //suppose x- ->y and y---> x and y--->/- ->*t and x is pruned; then y must be restarted
	discarding = 2
	;

struct strange_stack;
typedef unordered_map<shared_t,antichain_comb_t> vec_ac_t;
typedef strange_stack work_pq; //typedef unordered_priority_set<bstate_t> work_pq;

Breached_p_t Pre(const BState&, Net&);

void minprint_dot_search_graph(vec_ac_t&, unsigned, string, string, bstate_t mark = nullptr);
void init(vec_ac_t&, complement_vec*, const OState&, BState);
void make_target(bstate_t&, vec_ac_t&, work_pq&, lst_bs_t&, BState&);
void adjust(bstate_t pi, unsigned modei, bstate_t& t, vec_ac_t& M, work_pq& W, lst_bs_t& L, complement_vec& C, bool erase, Net& n, Breached_p_t& D);
void sync(vec_ac_t& M, complement_vec& C, work_pq& W, bstate_t& t, lst_bs_t& L, Net& n, Breached_p_t& D);
void minbw(Net*, const unsigned, complement_vec*);
void final_stats();

typedef pair<bstate_t,unsigned> wp_t;
struct strange_stack
{
	typedef bstate_t key_type;
	typedef unsigned data_type;
	
	typedef boost::bimap<bstate_t, boost::bimaps::multiset_of<unsigned> > bimap_t;
	typedef bimap_t::value_type value_type;

	bimap_t m;

	static const unsigned max = 100; 

	bool empty() const;
	void push(wp_t);
	void pop();
	wp_t top();
	void erase(wp_t);
	bool contains(wp_t) const;
	size_t size() const;
};
#endif

#ifdef MINBW_CC
#define EXTERN
ostream_sync bw_log(cerr.rdbuf()), bw_stats(cerr.rdbuf());
#else
#define EXTERN extern
extern ostream_sync bw_log, bw_stats;
#endif

EXTERN unsigned 
	ctr_pit, ctr_wit,
	ctr_msz, ctr_mszM,
	ctr_wsz, ctr_wszM,
	ctr_isold, ctr_isnew,
	ctr_nomin, ctr_ismin,
	ctr_csz, ctr_cszM, ctr_cszM_f,
	ctr_cdp, ctr_cdpM, ctr_cdpM_f,
	tmp_ctr; //no specific use
	;

EXTERN map<unsigned,unsigned>
	ctr_cszM_f_map,
	ctr_cdpM_f_map
	;
