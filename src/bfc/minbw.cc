#define MINBW_CC

#define WAIT_NOT_FOR_FW false

#include "minbw.h"

#include <boost/lexical_cast.hpp>
#include <fstream>
#include <stack>

extern volatile bool shared_fw_done; //TODO
extern volatile bool need_stats; //TODO
extern volatile bool shared_bw_done; //TODO
extern volatile bool shared_bw_finised_first; //TODO
extern bool bw_safe; //TODO
extern shared_cmb_deque_t shared_cmb_deque; //TODO

using namespace std;

#define UPDATE_STATS (ctr_msz = count_vertices(M), ctr_mszM = max(ctr_msz,ctr_mszM),\
	ctr_wsz = W.size(), ctr_wszM = max(ctr_wszM,ctr_wszM),\
	ctr_csz = w->size(), ctr_cszM = max(ctr_cszM,ctr_csz),\
	ctr_cdp = w->nb->gdepth, ctr_cdpM = max(ctr_cdpM,ctr_cdp))
#define UPDATE_STATS2(r) switch(r.case_type){\
case eq:			ctr_isold++; break;\
case neq_ge:		ctr_nomin++; break;\
case neq_le:		ctr_ismin++, ctr_isnew++; break;\
case neq_nge_nle:	ctr_ismin++, ctr_isnew++; break;}
#define LOG_OUTPUT1 (bw_log << "processing " << *w << " (" << W.size() << " remaining)" << endl)
#define LOG_OUTPUT2(p,r) bw_log << "checking predecessor " << *p << endl; \
	switch(r) {\
	case eq:		bw_log << "=       : delete (use existing)" << endl; break;\
	case neq_ge:	bw_log << ">       : delete (try wake-up)" << endl; break;\
	case neq_le:	bw_log << "<       : use and discard larger" << endl; break;\
	default:		bw_log << "< or != : use" << endl; break; }
#define INVARIANT1 invariant(std::all_of(r.ex.begin(),r.ex.end(),[&e,&W](bstate_t e){ return ((e->nb == nullptr) || (e->nb->status == BState::pending && W.contains(make_pair(e,e->size()))) || (e->nb->status == BState::processed && !W.contains(make_pair(e,e->size())))); }))

unsigned count_vertices(vec_ac_t& M)
{
	//TODO: do this only in debug mode; too expensive
	unsigned ret = 0;
	for_each(M.begin(),M.end(),[&ret](vec_ac_t::value_type& v){ 
		for_each(v.second.M.begin(),v.second.M.end(),[&ret](Breached_p_t::value_type b){
			if(!(b->nb == nullptr)) ++ret;});});
	return ret;
}

void minprint_dot_search_graph(vec_ac_t& M, unsigned ctr_wit, string ext, string base, bstate_t mark)
{
	bw_log << "writing uncoverability graph..." << endl;
	string out_fn = base + ".minbw.it_" + add_leading_zeros(boost::lexical_cast<string>(ctr_wit),5) + ext + ".dot";

	ofstream out(out_fn.c_str());
	if(!out.good()) throw logic_error(string("cannot write to ") + out_fn);

	out << "digraph BDD {" << endl;
	out << "rankdir = BT;" << endl;
	for(auto& u : M)
		for(auto& q : u.second.M)
			if(!(q->nb == nullptr)) 
			{

				bool min_state = true;
				for(auto& v : M)
					for(auto& s : v.second.M)
						if(!(s->nb == nullptr) && s != q && *s <= *q) 
						{
							min_state = false, bw_log << "state " << *q << " is not minimal; it covers " << *s << endl;
						}

				switch(graph_type){
				case GTYPE_DOT: q->mindot(out,q==mark), out << endl; break;
				case GTYPE_TIKZ: out << '"' << q->id_str() << '"' << ' ' << '[' << "lblstyle=" << '"' << "bwnode" << (min_state?",minstate":",nonminstate") << '"' << ",texlbl=" << '"' << q->str_latex() << '"' << ']' << ';' << "\n"; }
			}

	for(auto& u : M)
	{
		for(auto& q : u.second.M)
			if(!(q->nb == nullptr)) 
			{
				for(bstate_t r : q->nb->suc) //out << '"' << q->id_str() << '"' << " -> " << '"' << r->id_str() << '"' << " [" << "style=solid,arrowsize=\".75\"" << "];" << endl;
					switch(graph_type){
					case GTYPE_DOT: out << '"' << q->id_str() << '"' << " -> " << '"' << r->id_str() << '"' << "[style=solid];" << endl; break;
					case GTYPE_TIKZ: out << '"' << q->id_str() << '"' << " -> " << '"' << r->id_str() << '"' << "[style=predecessor_edge];" << endl; break;}

				for(bstate_t r : q->nb->csuc) //out << '"' << q->id_str() << '"' << " -> " << '"' << r->id_str() << '"' << " [" << "style=dotted,arrowsize=\".75\"" << "];" << endl;
					switch(graph_type){
					case GTYPE_DOT: out << '"' << q->id_str() << '"' << " -> " << '"' << r->id_str() << '"' << "[style=dotted];" << endl; break;
					case GTYPE_TIKZ: out << '"' << q->id_str() << '"' << " -> " << '"' << r->id_str() << '"' << "[style=coverpredecessor_edge];" << endl; break;}
			}
	}
	out << "}" << endl;
	out.close();

	string cmd = "dot -T pdf " + out_fn + " -o " + out_fn + ".pdf";
	(void)system(cmd.c_str());
	bw_log << cmd << endl;
}

Breached_p_t Pre(const BState& s, Net& n)
{

	Breached_p_t pres; //set of successors discovered for s so far (a set is used to avoid reprocessing)
	
	static pair<Net::vv_adjs_t,Net::vvl_adjs_t> T = n.get_backward_adj_2();
	Net::vv_adjs_t& X = T.first;
	Net::vvl_adjs_t& Y = T.second;
	
	stack<const BState*> work; work.push(&s); //todo: allocate nb and bl here already?

	while(!work.empty())
	{

		const BState* cur = work.top(); work.pop();

		if(cur != &s && n.core_shared[cur->shared])
		{
			if(!pres.insert(cur).second)
				delete cur;

			continue;
		}

		invariant(iff(cur == &s, n.core_shared[cur->shared]));

		vector<pair<vector<Transition>::const_iterator,vector<Transition>::const_iterator> > tranges;
		tranges.push_back(make_pair(X[cur->shared].begin(),X[cur->shared].end()));
		for_each(cur->bounded_locals.begin(),cur->bounded_locals.end(),[&tranges,&cur,&X,&Y](const bounded_t::value_type& b){tranges.push_back(make_pair(Y[cur->shared][b].begin(),Y[cur->shared][b].end()));});

		for( auto& r : tranges )
		{
			for( ;r.first != r.second; ++r.first)
			{
				const Transition&	t = *r.first;
				const Thread_State& u = t.source;
				const Thread_State& v = t.target; 
				const transfers_t&	p = t.bcs; //inverse transfers (backward)
				const transfers_t&	p2= t.bcs2; //transfers (forward)

				invariant(cur->shared == v.shared);

				const bool thread_in_u = (cur->bounded_locals.find(u.local) != cur->bounded_locals.end());
				const bool thread_in_v = (cur->bounded_locals.find(v.local) != cur->bounded_locals.end());
				const bool horiz_trans = (u.shared == v.shared);
				const bool diff_locals = (u.local != v.local);

				invariant(!horiz_trans || thread_in_v || !p.empty());

				if(horiz_trans && !thread_in_v && cur->bounded_locals.empty())
					continue;

				BState* pre = new BState(horiz_trans?cur->shared:u.shared,cur->bounded_locals.begin(),cur->bounded_locals.end(),false); //allocation

				if(thread_in_v) pre->bounded_locals.erase(pre->bounded_locals.find(v.local)); //remove one

				if(0/*some passive local is force to move in the transition*/) //if some passive local is in conflict with the broadcast effect
					continue;

				//create, for all passive updates combinations c_i in bounded local states, a state s_i = s+c_i (check for duplicates, and ignoring those entering an "omega"-value)
				typedef pair<bounded_t,bounded_t> local_tp; //unprocessed and processed local states
				typedef set<local_tp> comb_t;
				bool first = false;
				if(!p.empty() && !pre->bounded_locals.empty()) //if there are passive updates and at least one passive thread exists
				{
					first = true;
					comb_t combs_interm; 
					stack<comb_t::const_iterator> cwork;
					cwork.push(combs_interm.insert(make_pair(pre->bounded_locals,bounded_t())).first); //inserte pair (locals,{})

					while(!cwork.empty())
					{
						comb_t::const_iterator w = cwork.top(); cwork.pop();
						invariant(w->first.size()); //work shall only contain states with unprocessed local states

						set<local_t> DEST; //TODO: inefficient: avoid to copy x->second
						local_t passive_source = *w->first.begin();
						if(p2.find(passive_source) == p2.end()) // not p ~> *, thus p ~> p, which has inverse p <~ p
							DEST.insert(passive_source);
						auto x = p.find(passive_source);
						if(x != p.end())
							DEST.insert(x->second.begin(),x->second.end()); //add other inverse transfers

						//remove one local and create a new pair for all predecessor combinations
						for(auto& passive_target : DEST)
						{
							local_tp ne(*w);
							ne.second.insert(passive_target);
							invariant(*ne.first.begin() == passive_source);
							ne.first.erase(ne.first.find(passive_source));

							if(ne.first.empty())
							{
								//create new states for all but the first one; the first one is added in the remainder of this function
								if(first)
									//do not readd active thread (done below)
										pre->bounded_locals = ne.second, first = false;
								else
								{
									ne.second.insert(u.local);
									BState* prep = new BState(pre->shared,ne.second.begin(),ne.second.end(),false); //allocation
									invariant(prep->consistent());
									work.push(prep);
								}
							}
							else
							{
								auto a = combs_interm.insert(ne);
								if(a.second) cwork.push(a.first);
							}
						}
						//while(x2 != p2.end() && ++pi != x->second.end() && (passive_target = *pi,1));
						//while(x != p.end() && ++pi != x->second.end() && (passive_target = *pi,1));
						

					}
				}

				if(!first)
				{
					pre->bounded_locals.insert(u.local);
					work.push(pre);
				}
				else
				{
					//no predecessor combination found
				}
			}
		}

		if(cur != &s) 
			delete cur;

	}

	return pres;

}

void init(vec_ac_t& M, complement_vec* C, Net& n)
{
	bw_log << "Initializing..." << endl;
	BState* q;
	for(shared_t s = 0; s < BState::S && (!shared_fw_done || WAIT_NOT_FOR_FW) && exe_state == RUNNING; ++s)
		for(auto b = C->luv[s].u_nodes.begin(), e = C->luv[s].u_nodes.end(); n.core_shared[s] && b != e && (!shared_fw_done || WAIT_NOT_FOR_FW) && exe_state == RUNNING; ++b) //traverse all minimal uncovered elements (e.g. 0|1, 1|0 and 1|1 for initial state 0|0w, S=L=2; independant of k)
		{
			if(!(BState(s,(*b)->c.begin(),(*b)->c.end()) <= n.target))
			{
				q = new BState(s,(*b)->c.begin(),(*b)->c.end()), M[q->shared].insert_incomparable(q), bw_log << *q << " added" << endl, invariant(q->nb == nullptr);
			}
			else
			{
				bw_log << "ignore " << BState(s,(*b)->c.begin(),(*b)->c.end()) << ", it's smaller than the target" << endl; //as a workaround, don't try smaller elements for the initial state (there is some bug related to replacing the target state on-the-fly)
			}
		}
}

void make_target(bstate_t& t, vec_ac_t& M, work_pq& W, lst_bs_t& L, Net& n)
{
	bw_log << "create target vertex" << endl;
	po_rel_t r = M[n.target.shared].relation(&n.target);

	//invariant(r == eq || r == neq_ge || r == neq_nge_nle || r == neq_le__or__neq_nge_nle); //(i) the target has <= k threads, and thus a node already exists, or (ii) a state with <= k threads that is strictly smaller than the target is also uncoverable, or (iii) no uncoverable state with <= k threads exists that is smaller than the target
	invariant(r == neq_le__or__neq_nge_nle);
	switch(r){
	case eq:										t = *M[n.target.shared].case_insert(&n.target).ex.begin(), bw_log << "target already exists -> use it" << endl; break; //take existing element as target
	case neq_ge:									t = *M[n.target.shared].case_insert(&n.target).ex.begin(), bw_log << "smaller target exists -> use it" << endl;	break; //take some strictly smaller existing element as target
	case neq_nge_nle: case neq_le__or__neq_nge_nle:	t = new BState(n.target), M[n.target.shared].insert_incomparable(t), bw_log << "no smaller target exists -> added" << endl; break; } //add a new node for the target
	
	invariant(t->nb == nullptr), t->nb = new BState::neighborhood_t, t->nb->status = BState::pending, t->nb->li = L.insert(L.end(),t), W.push(make_pair(t,t->size())), t->nb->gdepth = 0, t->nb->ini = *t <= n.init;
	
	//TODO: use this!!!
	//invariant(t->nb == nullptr), t->nb = new BState::neighborhood_t, t->nb->ini = *t <= n.init, t->nb->status = BState::pending, t->nb->li = L.insert(L.end(),t), W.push(make_pair(t,t->nb->ini?work_pq::max:t->size())), t->nb->gdepth = 0;

	bw_log << "target node created: " << *t << endl;
}

void mark_reachable(bstate_t t, unsigned flag)
{
	//mark all states that are reachable from the target via --->/- - -> edges (only unmarked states will be discarded)
	typedef unordered_set<bstate_t> bstate_ptr_set; //compares pointer value, not states
	bstate_ptr_set seen; seen.insert(t);
	stack<bstate_t> work; work.push(t); //todo: allocate nb and bl here already?

	bstate_t p, u;
	while(!work.empty())
	{
		p = work.top(); work.pop();
		invariant(!(p->fl == flag));
		p->fl = flag, bw_log << *p << " marked " << flag << endl;
		auto& pre = p->nb->pre, &cpre = p->nb->cpre;
		for(auto ui = pre.begin(); ui != pre.end() && (u=*ui,1); ui++)
			if(seen.insert(u).second) work.push(u), bw_log << *u << " added to stack (" << *u << "---->" << *p << " )" << endl;
		for(auto ui = cpre.begin(); ui != cpre.end() && (u=*ui,1); ui++)
			if(seen.insert(u).second) work.push(u), bw_log << *u << " added to stack (" << *u << "- - >" << *p << " )" << endl;
	}				
}

bool presuc_inv(vec_ac_t& M)
{
	for(auto& u : M)
	{
		for(auto& q : u.second.M)
			if(!(q->nb == nullptr)) 
			{
				for(bstate_t r : q->nb->suc) 
					if(r->nb->pre.find(q) == r->nb->pre.end()) 
						return false;
				for(bstate_t r : q->nb->csuc)
					if(r->nb->cpre.find(q) == r->nb->cpre.end()) 
						return false;
				for(bstate_t r : q->nb->pre)
					if(r->nb->suc.find(q) == r->nb->suc.end()) 
						return false;
				for(bstate_t r : q->nb->cpre)
					if(r->nb->csuc.find(q) == r->nb->csuc.end()) 
						return false;
			}
	}
	return true;
}

void adjust(bstate_t pi, unsigned modei, bstate_t& t, vec_ac_t& M, work_pq& W, lst_bs_t& L, complement_vec& C, bool erase, Net& n, Breached_p_t& D)
{
	static int adjust_num = 0;

	//to avoid problems in the presence of cycles prefer "restarting" nodes
	wp_t w;
	bstate_t& p = w.first;
	unsigned& m = w.second;

	strange_stack work; 
	work.push(make_pair(pi,modei));

	bool first = true, new_target = false;

	switch(modei){
	case discarding: bw_log << "mode: discarding" << endl; break;
	case pruning: bw_log << "mode: pruning" << endl; break;
	case restarting: bw_log << "mode: restarting" << endl; break; }

	while(!work.empty())
	{

		bw_log << "---- adjust iteration ----" << endl;

		w = work.top(), work.pop(), bw_log << "adjust " << *p << endl;

		invariant(iff(M[p->shared].manages(p),(pi != p) || (erase && pi == p)));
		invariant((p->nb == nullptr) || (!(p->nb == nullptr) && p->nb->status == BState::pending && W.contains(make_pair(p,p->size()))) || (!(p->nb == nullptr) && p->nb->status == BState::processed && !W.contains(make_pair(p,p->size()))));

		bstate_t u;
		switch(m)
		{
		case discarding: bw_log << "discarding" << endl;
			
			if(modei == pruning && first && !new_target && !(t == nullptr)) //if the last either of the last two conjuncts does not hold, then all nodes are discarded
				first = false, mark_reachable(t,1);

			if(p->fl)
			{
				bw_log << *p << " marked reachable -> discard cancelled" << endl;
				continue;
			}
			//fall-through
		case pruning:  bw_log << "pruning" << endl;
			if(!(p->nb == nullptr))
			{
				auto& suc = p->nb->suc, &csuc = p->nb->csuc;
				for(auto ui = suc.begin(); ui != suc.end() && (u=*ui,1); ui = suc.erase(ui))
				{
					u->nb->pre.erase(p), bw_log << "remove " << *p << "---->" << *u << endl;
					if(m == pruning) work.push(make_pair(u,pruning)), bw_log << *u << " added as 'pruning'" << endl;
				}
				for(auto ui = csuc.begin(); ui != csuc.end() && (u=*ui,1); ui = csuc.erase(ui))
				{
					u->nb->cpre.erase(p), bw_log << "remove " << *p << "- - >" << *u << endl;
					if(m == pruning) work.push(make_pair(u,restarting)), bw_log << *u << " added as 'restarting'" << endl;
				}
			}
			if(!(p->nb == nullptr))
			{
				auto& pre = p->nb->pre, &cpre = p->nb->cpre;
				for(auto ui = pre.begin(); ui != pre.end() && (u=*ui,1); ui = pre.erase(ui))
				{
					u->nb->suc.erase(p), bw_log << "remove " << *u << "---->" << *p << endl;
					work.push(make_pair(u,discarding)), bw_log << *u << " added as 'discarding'" << endl;
				}
				for(auto ui = cpre.begin(); ui != cpre.end() && (u=*ui,1); ui = cpre.erase(ui))
				{
					u->nb->csuc.erase(p), bw_log << "remove " << *u << "- - >" << *p << endl;
					work.push(make_pair(u,discarding)), bw_log << *u << " added as 'discarding'" << endl;
				}
			}

			invariant((p->nb == nullptr) || ((p->nb->pre.empty()) && (p->nb->cpre.empty()) && (p->nb->suc.empty()) && (p->nb->csuc.empty()))); //vertices satisfying p->nb == nullptr may be reported by the forward search
			if(erase || p != pi) 
				M[p->shared].erase(p), bw_log << "erase " << *p << endl;
			if(!(p->nb == nullptr))
			{
				L.erase(p->nb->li), bw_log << "de-list " << *p << endl;
				if(p->nb->status == BState::pending) 
					W.erase(make_pair(p,p->size())), bw_log << "de-work " << *p << endl;
			}

			if(m == pruning && p->size() <= C.K) //must be done after p was removed from M, but before a new target is inserted and p is deleted
			{
				bool added;
				++ctr_pit;
				for(const cmb_node_p n_ : C.diff_insert(p->shared,cmb_node(p->bounded_locals.begin(),p->bounded_locals.end(),0,nullptr)).second)
					invariant(!M[p->shared].manages(p)),u = new BState(p->shared,n_->c.begin(),n_->c.end()), added = M[u->shared].insert(u).second, invariant(added), invariant(u->nb == nullptr), bw_log << *u << " added" << endl;
			}

			if(p == t && *t == n.target) t = nullptr, bw_log << "target is coverable, set to null" << endl;
			else if(p == t) make_target(t,M,W,L,n), new_target = true;

			if(m == pruning) D.insert(p), bw_log << "marked coverable" << endl;
			else delete p, bw_log << "deleted" << endl;

			break;

		case restarting: bw_log << "restarting" << endl;
			for(auto ui = p->nb->li; ui != L.end() && (u=*ui,1); ++ui)
				if(u->nb->status == BState::processed) 
					u->nb->status = BState::pending, W.push(make_pair(u,u->size())), bw_log << "restarting " << *u << endl;
		}
	}

	if(first == false)
		mark_reachable(t,0);
}

void sync(vec_ac_t& M, complement_vec& C, work_pq& W, bstate_t& t, lst_bs_t& L, Net& n, Breached_p_t& D)
{
	static BState	b; //workbench
	while(!shared_cmb_deque.lst->empty() && (!shared_fw_done || WAIT_NOT_FOR_FW) && exe_state == RUNNING)
	{
		bw_log << "+++++++++++++++ Sync iteration " << ctr_wit << " +++++++++++++++" << endl;
		mu_list::lst_t* m = new mu_list::lst_t;
		shared_cmb_deque.mtx.lock(), swap(shared_cmb_deque.lst,m), shared_cmb_deque.mtx.unlock();
		for(auto& s : *m)
		{
			b.shared = s.first, b.bounded_locals.clear(), b.bounded_locals.insert(s.second->c.begin(),s.second->c.end());
			bw_log << "got " << b << endl;
			if(M[b.shared].manages(&b)) 
				adjust(*M[b.shared].case_insert(&b).ex.begin(),pruning,t,M,W,L,C,true,n,D); //get existing node and remove it from M
			else 
				bw_log << b << " already known" << endl, invariant(implies(!(b <= *t),D.find(&b) != D.end()));
		}
		delete m;
	}
}

void final_stats()
{
	shared_bw_done = true;
	
	fwbw_mutex.lock(), (shared_fw_done) || (shared_bw_finised_first = 1), fwbw_mutex.unlock();

	if(shared_bw_finised_first) bw_log << "bw first" << "\n", bw_log.flush();
	else bw_log << "bw not first" << "\n", bw_log.flush();

	{
		bw_stats << endl;
		bw_stats << "---------------------------------" << endl;
		bw_stats << "Backward statistics:" << endl;
		bw_stats << "---------------------------------" << endl;
		bw_stats << "bw finished first               : " << (shared_bw_finised_first?"yes":"no") << endl;
		bw_stats << "bw execution state              : "; 
		switch(exe_state){
		case TIMEOUT: bw_stats << "TIMEOUT" << endl; break;
		case MEMOUT: bw_stats << "MEMOUT" << endl; break;
		case RUNNING: bw_stats << "RUNNING" << endl; break;
		case INTERRUPTED: bw_stats << "INTERRUPTED" << endl; break;}
		bw_stats << endl;
		bw_stats << "iterations                      : " << ctr_wit + ctr_pit << endl;
		bw_stats << "- work iterations               : " << ctr_wit << endl;
		bw_stats << "- prune iterations              : " << ctr_pit << endl;
		bw_stats << endl;
		bw_stats << "new predecessors                : " << ctr_isnew << endl;
		bw_stats << "existing predecessors           : " << ctr_isold << endl;
		bw_stats << "minimal                         : " << ctr_ismin << endl;
		bw_stats << "non-minimal                     : " << ctr_nomin << endl;
		bw_stats << "---------------------------------" << endl;		
		bw_stats << "minimal vertices (max)          : " << ctr_mszM << endl;
		bw_stats << "working set size (max)          : " << ctr_wszM << endl;
		bw_stats << "---------------------------------" << endl;		
		bw_stats << "backward depth (max)            : " << ctr_cdpM << endl;
		bw_stats << "backward width (max)            : " << ctr_cszM << endl;
		bw_stats << "backward depth (max,final)      : " << ctr_cdpM_f << endl;
		bw_stats << "backward width (max,final)      : " << ctr_cszM_f << endl;
		bw_stats << "---------------------------------" << endl; 
		for(auto a : ctr_cszM_f_map) 
		bw_stats << "num width " << a.first << " (final)             : " << a.second << endl;
		for(auto a : ctr_cdpM_f_map) 
		bw_stats << "num depth " << a.first << " (final)             : " << a.second << endl;
		bw_stats << "---------------------------------" << endl; 
		bw_stats << "tmp                             : " << tmp_ctr << endl;
		bw_stats << "---------------------------------" << endl; 
		bw_stats.flush();
	}
}

extern void disable_mem_limit();
void minbw(Net* n, const unsigned k, complement_vec* C)
{
	try
	{
		work_pq							W; //working set
		vec_ac_t						M; //ensure n->target is incomparable to all other nodes
		lst_bs_t						L; //doubly liked list over all pending and processed nodes in M (in insertion order)
		Breached_p_t					D; //set of known-to-be-coverable states (currently not downward-close, so states may be reexplored unneccessarily)

		bstate_t						w = nullptr;	//work node
		bstate_t						t = nullptr;	//target node
		bstate_t						p = nullptr;	//predecessor node

		Breached_p_t					P;				//set of predecessor
		Breached_p_t::const_iterator	i,e;			//predecessor iterators

		init(M,C,*n);

		make_target(t,M,W,L,*n);

		bw_log << "Searching..." << endl, bw_log.flush();
		for(; !W.empty() && (!shared_fw_done || WAIT_NOT_FOR_FW) && exe_state == RUNNING; sync(M,*C,W,t,L,*n,D))
		{
			//boost::this_thread::sleep(boost::posix_time::milliseconds(100));
			ctr_wit++;
			bw_log << "+++++++++++++++ Work set iteration " << ctr_wit << " +++++++++++++++" << endl;
			//if(graph_type != GTYPE_NONE) minprint_dot_search_graph(M,ctr_wit,"_wit",n->filename,w = W.top().first);
			
			w = W.top().first, W.pop(), invariant(!(w->nb == nullptr) && w->nb->status == BState::pending), w->nb->status = BState::processed;

			invariant(iff(w->nb->ini,(D.find(w) != D.end() || *w <= n->init)));
			if(M[w->shared].relation(w,true) == neq_ge)
			{
				if(w->nb->pre.empty() && w->nb->cpre.empty())
					bw_log << "discarding work node " << *w << " (not minimal anymore)" << endl, adjust(w,discarding,t,M,W,L,*C,true,*n,D);
				else
					++tmp_ctr; //this case is very rare

				//TODO: not clear whether we can prune here or not
				//if(!(w->nb->pre.empty() && w->nb->cpre.empty()))
				//	throw logic_error("case not supported");
				continue;
			}
			if(need_stats) UPDATE_STATS, need_stats = false;
			LOG_OUTPUT1;

			if(w->nb->ini)
			{
				bw_log << "coverable" << endl, adjust(w,pruning,t,M,W,L,*C,true,*n,D); //note: p coverable does not imply w is coverable!
				continue;
			}

			for(P = Pre(*w, *n), i = P.begin(), e = P.end(); i != e && (p = *i,1) && (i = P.erase(i),1);)
			{
				insert_t r = M[p->shared].case_insert(p); UPDATE_STATS2(r); LOG_OUTPUT2(p,r.case_type); INVARIANT1;

				if(r.case_type == eq || r.case_type == neq_ge) delete p;

				if(r.case_type == eq) p = *r.ex.begin();
				else if(r.case_type == neq_ge) if(!all_of(r.ex.begin(),r.ex.end(),[&p,&r](bstate_t e){ return (e->nb == nullptr) && (p = e,1); })) continue;
				else if(r.case_type == neq_le) for(const bstate_t& e : r.ex) adjust(e,discarding,t,M,W,L,*C,false,*n,D);

				if(p->nb == nullptr) p->nb = new BState::neighborhood_t, p->nb->ini = D.find(p) != D.end() || *p <= n->init, p->nb->status = BState::pending, p->nb->li = L.insert(L.end(),p), W.push(make_pair(p,p->nb->ini?work_pq::max:p->size())), p->nb->gdepth = w->nb->gdepth + 1, bw_log << "created new vertex" << endl;
				
				if(r.case_type == neq_ge) w->nb->cpre.insert(p), p->nb->csuc.insert(w), bw_log << "edge " << *p << "- - >" << *w  << endl;
				else w->nb->pre.insert(p), p->nb->suc.insert(w), bw_log << "edge " << *p << "---->" << *w  << endl;
			}
		}

		if(graph_type != GTYPE_NONE) minprint_dot_search_graph(M,0,"_final",n->filename);

		invariant(implies(t == nullptr,W.empty()));
		if(t == nullptr)
			bw_safe = false;
		else 
			bw_safe = true;

		////TEST
		//unsigned min_ctr = 0, non_min_ctr = 0;
		//for(auto& u : M)
		//{
		//	for(auto& q : u.second.M)
		//	{
		//		if(!(q->nb == nullptr) && q->nb->status == BState::processed) 
		//		{
		//			bool min_state = true;
		//			for(auto& v : M)
		//			{
		//				for(auto& s : v.second.M)
		//					if(!(s->nb == nullptr) && s->nb->status == BState::processed && s != q && *s <= *q)
		//					{
		//						min_state = false;
		//					}
		//			}
		//			if(min_state) ++min_ctr;
		//			else ++non_min_ctr;
		//		}
		//	}
		//}
		//cout << "min_ctr: " << min_ctr << endl;
		//cout << "non_min_ctr: " << non_min_ctr << endl;

		//clean up M and update statistics
		for(auto& b : M)
			for(auto& c : b.second.M)
			{
				if(c->nb != nullptr && c->nb->status == BState::processed)
				{
					ctr_cdpM_f = max(ctr_cdpM_f,c->nb->gdepth),ctr_cszM_f = max(ctr_cszM_f,(unsigned)c->size());
					++ctr_cdpM_f_map[c->nb->gdepth], ++ctr_cszM_f_map[c->size()];
				}
				delete c;
			}

		//clean up D
		for(bstate_t b : D)
			delete b;

	}
	catch(std::bad_alloc)
	{
		cerr << fatal("backward exploration reached the memory limit") << "\n"; 
		exe_state = MEMOUT;
#ifndef WIN32
		disable_mem_limit(); //to prevent bad allocations while printing statistics
#endif
	}

	final_stats();
}

/****************
Strange (non-)stack
****************/
bool strange_stack::empty() const
{ 
	return m.empty(); 
}

void strange_stack::push(wp_t e)
{
	auto f = m.left.find(e.first);
	if(f == m.left.end()) m.insert(value_type(e.first,e.second));
	else if(f->second > e.second) m.left.replace_data(f,e.second);
}

void strange_stack::pop()
{ 
	m.right.erase(m.right.begin()); 
}

wp_t strange_stack::top()
{
	auto f = m.right.begin();
	return make_pair(f->second,f->first);
}

void strange_stack::erase(wp_t e)
{
	size_t old = size();
	auto f = m.left.find(e.first);
	invariant(f !=  m.left.end());
	m.left.erase(f);
	invariant(size() < old);
}

bool strange_stack::contains(wp_t e) const
{
	return m.left.find(e.first) != m.left.end();
}

size_t strange_stack::size() const
{
	return m.left.size();
}