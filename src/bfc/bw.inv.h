/********************************************************
Consistency
********************************************************/

bool intersection_free(const vec_antichain_t& D, const vec_antichain_t& M)
{
	set<bstate_t> BBs1;
	for(shared_t s = 0; s < BState::S; ++s)
		foreach(bstate_t b,M.uv[s].M) BBs1.insert(b);

	for(shared_t s = 0; s < BState::S; ++s)
		foreach(bstate_t b,D.uv[s].M){
			if(BBs1.find(b) != BBs1.end())
				return false;}

	return true;
}

bool is_consistent(const work_pq& W)
{

	foreach(const work_pq::unordered_key_types& u, W.v_us)
		foreach(const work_pq::key_type& k, u)
			if(k->nb->status != BState::pending)
				return false;

	return true;
}

bool source_U_minimality(vec_antichain_t& M, non_minimals_t& N)
{
	foreach(const antichain_t& se, M.uv){
		foreach(const bstate_t& n, se.M_cref()){
			if(n->nb->src == n){
				stack<bstate_t> S; S.push(n);
				while(!S.empty()){
					bstate_t q = S.top(); S.pop();

#ifdef EAGER_ALLOC
					if(!q->nb->src->us->uv[q->shared].manages(q))
#else
					if(q->nb->src->us != nullptr && !q->nb->src->us->uv[q->shared].manages(q))
#endif
						return false;

					if(!M.uv[q->shared].manages(q) && N.find(q) == N.end())
						return false;

					foreach(bstate_t s, q->nb->suc)
						if(s->nb->src == q->nb->src)
							S.push(s);
				}
			}
		}
	}

	return true;
}

bool U_consistent(vec_antichain_t& M)
{
	foreach(const antichain_t& se, M.uv){
		foreach(const bstate_t& n, se.M_cref()){
			if(n->nb->status != BState::pending && n->nb->status != BState::processed)
				return false;
			if(n->nb->status == BState::pending && !n->nb->suc.empty())
				return false;
			if(n->nb->status == BState::blocked_pending && !n->nb->suc.empty())
				return false;
			if(!n->consistent(BState::full_check)) 
				return false;
#ifdef EAGER_ALLOC
			if(!n->nb->src->us->uv[n->shared].manages(n)) 
				return false;
#else
			if(n->nb->src->us != nullptr && !n->nb->src->us->uv[n->shared].manages(n)) 
				return false;
#endif
			foreach(const bstate_t& m, n->nb->suc){
				if(!m->consistent(BState::full_check)) 
					return false;
#ifdef EAGER_ALLOC
				if(!m->nb->src->us->uv[m->shared].manages(m)) 
#else
				if(m->nb->src->us != nullptr && !m->nb->src->us->uv[m->shared].manages(m)) 
#endif
					return false;
			}
#ifdef EAGER_ALLOC
			if(n->nb->src == n){ //source
#else
			if(n->nb->src == n && n->nb->src->us != nullptr){ //source
#endif
				foreach(const antichain_t& s, n->nb->src->us->uv){
					foreach(const bstate_t& q, s.M_cref()){
						if(!q->consistent(BState::full_check))
							return false;
					}
				}
			}
		}
	}

	return true;
}

bool small_U_consistent(vec_antichain_t& M)
{
	foreach(const antichain_t& se, M.uv){
		foreach(const bstate_t& n, se.M_cref()){
#ifdef EAGER_ALLOC
			if(n->nb->src == n){ //source
#else
			if(n->nb->src == n && n->nb->src->us != nullptr){ //source
#endif
				foreach(const antichain_t& s, n->nb->src->us->uv){
					foreach(const bstate_t& q, s.M_cref()){
						if(!q->consistent(BState::full_check))
							return false;
					}
				}
			}
		}
	}

	return true;
}

bool not_local_U_referenced(bstate_t p, vec_antichain_t& M){
	foreach(const antichain_t& se, M.uv){
		foreach(const bstate_t& n, se.M_cref()){
#ifdef EAGER_ALLOC
			if(n->nb->src == n || n->nb->ini){ //source
#else
			if(n->nb->src->us != nullptr && (n->nb->src == n || n->nb->ini)){ //source
#endif
				foreach(const antichain_t& s, n->nb->src->us->uv){
					if(s.manages(p))
						return false;
				}
			}
		}
	}

	return true;
}

bool no_unconnected_local_U_references(vec_antichain_t& M)
{
	foreach(const antichain_t& se, M.uv){
		foreach(const bstate_t& n, se.M_cref()){
#ifdef EAGER_ALLOC
			if(n->nb->src == n || n->nb->ini){ //source
#else
			if((n->nb->src == n || n->nb->ini) && n->us != nullptr){ //source
#endif
				foreach(const antichain_t& s, n->us->uv){
					foreach(bstate_t pp, s.M_cref()){
						if(pp->nb->ini)
							continue;

						bool predec_with_src_n = false;
						foreach(bstate_t pre, pp->nb->pre){
							if(pre->nb->src == n){
								predec_with_src_n = true;
								break;
							}
						}
						if(!predec_with_src_n) 
							return false;
					}
				}
			}
		}
	}

	return true;
}

bool consistent(vec_antichain_t& M, non_minimals_t& N, vec_antichain_t& O, work_pq& W, complement_vec_t& C)
{
	//Check invariants
	foreach(bstate_t n, N){
		if(n->nb->status != BState::blocked_pending && n->nb->status != BState::blocked_processed)
			return false;
		if(n->nb->status == BState::blocked_pending && !n->nb->suc.empty())
			return false;
		if(!n->consistent(BState::full_check)) 
			return false;
#ifdef EAGER_ALLOC
		if(!n->nb->src->us->uv[n->shared].manages(n)) 
#else
		if(n->nb->src->us != nullptr && !n->nb->src->us->uv[n->shared].manages(n)) 
#endif
			return false;
		foreach(const bstate_t& m, n->nb->suc)
#ifdef EAGER_ALLOC
			if(!m->nb->src->us->uv[m->shared].manages(m)) 
#else
			if(m->nb->src->us != nullptr && !m->nb->src->us->uv[m->shared].manages(m)) 
#endif
				return false;
		foreach(const bstate_t& b, n->bl->blocks)
			if(!(*n <= *b && !(*n == *b))) 
				return false;
		foreach(const bstate_t& b, n->bl->blocked_by)
			if((*n <= *b && !(*n == *b))) 
				return false;
	}

	if(!U_consistent(M))
		return false;

	if(!no_unconnected_local_U_references(M))
		return false;

	if(!source_U_minimality(M, N))
		return false;

	foreach(const antichain_t& se, O.uv){
		foreach(const bstate_t& n, se.M_cref()){
			if(M.manages(n))
				return false;
		}
	}

	foreach(const antichain_t& se, M.uv){
		foreach(const bstate_t& n, se.M_cref()){
			if(O.manages(n))
				return false;
			if(!n->consistent(BState::full_check)) 
				return false;
#ifdef EAGER_ALLOC
			if(!n->nb->src->us->uv[n->shared].manages(n)) 
#else
			if(n->nb->src->us != nullptr && !n->nb->src->us->uv[n->shared].manages(n)) 
#endif
				return false;
			foreach(const bstate_t& m, n->nb->suc){
				if(!m->consistent(BState::full_check)) 
					return false;
#ifdef EAGER_ALLOC
				if(!m->nb->src->us->uv[m->shared].manages(m)) 
#else
				if(m->nb->src->us != nullptr && !m->nb->src->us->uv[m->shared].manages(m)) 
#endif
					return false;
			}
#ifdef EAGER_ALLOC
			if(n->nb->src == n){ //source
#else
			if(n->nb->src->us != nullptr && n->nb->src == n){ //source
#endif
				foreach(const antichain_t& s, n->nb->src->us->uv){
					foreach(const bstate_t& q, s.M_cref()){
						if(!q->consistent(BState::full_check))
							return false;
					}
				}
			}
			foreach(const bstate_t& b, n->bl->blocks)
				if(!(*n <= *b && !(*n == *b))) 
					return false;
			foreach(const bstate_t& b, n->bl->blocked_by)
				if((*n <= *b && !(*n == *b))) 
					return false;
		}
	}

	foreach(const work_pq::unordered_key_types& u, W.v_us){
		foreach(const work_pq::key_type& s, u){
			//invariant (INV_ndipl): each pointer occurs only once in the W list; note that two equal states with different addresses can however occur in case a state was marked as "pruned" while the same state is rediscovered again
			if(!(is_consistent(W))) 
				return false;
			//invariant (INV_stat): the status of every state in the worklist is either "pending" or "suspended"
			if(!(s->nb->status == BState::pending)) 
				return false;
			//invariant (INV_wlist_np): no state in the worklist is a null pointer
			if(!(s != nullptr)) 
				return false;
			//invariant (INV_cons): every state in the worklist is consistent
			if(!(s->consistent(BState::full_check))) 
				return false;
			//invariant (INV_min): every state in the worklist that is not marked for deallocation/suspension is minimal, and in this case therefore managed by M, otherwise it s not managed by M
			if(!(s->nb->status == BState::pending == M.uv[s->shared].manages(s))) 
				return false;
			//invariant (INV_wi): no state that is smaller than n.init exists in the W queue 
			if(!(!(*s <= net.init))) 
				return false;
			//invariant (INV_kcov): no state in the worklist is in the k-cover obtained so far (and hence not smaller than another element in the worklist). Example: suppose s=4|1 was replaced after pruning by s^=4|1,1 and we later again find a=4|1 within another source tree. Then, we neeed to prune a since (a leq_neq s^).
			cmb_node tmp(s->bounded_locals.begin(),s->bounded_locals.end(),0,NULL);
			if(!(C.luv[s->shared].l_nodes.find(&tmp) == C.luv[s->shared].l_nodes.end())) 
				return false; //note: not sure whether this only holds if s->nb->src != to_be_freed
			//invariant (INV_nblocked): no unsuspended state in the worklist is blocked by another state
			if(!(s->bl->blocked_by.empty())) 
				return false;
			//invariant: source states have an associated upperset, non-source states do not
#ifdef EAGER_ALLOC
			if(!((s->nb->ini) == (s->us != nullptr)))
				return false;
#endif
			if(s->nb->src == s && !s->nb->ini)
				return false;
			//invarient (INV_srcmng): a source is always contained in its upper set
#ifdef EAGER_ALLOC
			if(!((s->nb->src != s) || s->us->uv[s->shared].manages(s))) 
#else
			if(s->us != nullptr && !((s->nb->src != s) || s->us->uv[s->shared].manages(s))) 
#endif
				return false;
		}
	}

	return true;
}

bool before_prune(bstate_t p, vec_antichain_t& M, non_minimals_t& N, vec_antichain_t& O, pending_t& W, complement_vec_t& C)
{
	if(!consistent(M,N,O,W,C)) return false;
	if((!p->nb->ini && p->nb->src == p)) return false;
	if(!(p->nb->status == BState::pending || p->nb->status == BState::processed)) return false;
	if(!(M.manages(p) && N.find(p) == N.end())) return false; //the pruned state is globally minimal
	return true;
}
