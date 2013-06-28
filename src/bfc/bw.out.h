/********************************************************
Output
********************************************************/
static const string 
	DOT_local_edge_style = "style=solid,arrowsize=\".75\"",
	DOT_non_local_edge_style = "style=solid,arrowhead=dot,arrowsize=\".75\"",
	DOT_blocks_edge_style = "style=dashed,arrowhead=tee,arrowsize=\"1.25\"",

	DOT_source_shape = "box",
	DOT_non_source_shape = "diamond",
	DOT_current_style = "rounded",

	DOT_pending_fillcolor = "grey",
	DOT_processed_fillcolor = "green",
	DOT_prune_fillcolor = "black",

	DOT_nonblocked_style = "filled",
	DOT_blocked_style = "filled,diagonals",
	DOT_prune_style = "filled",

	DOT_non_prune_fontcolor = "black",
	DOT_prune_fontcolor = "white";

ofstream& dot_state_out(bstate_t q, bstate_t p, const work_pq& P, bool prune, graph_type_t gt, ofstream& out)
{

	out << '[';
	switch(gt)
	{
	case GTYPE_NONE: assert(0); break;
	case GTYPE_TIKZ:
		{
			out << "lblstyle=" << '"';

			if(q->nb->ini) out << "query,";
			else out << "nonquery,";

			if(q == p) out << "pending,";
			else switch(q->nb->status){
			case BState::pending: case BState::blocked_pending: out << "pending,"; break;
			case BState::processed: case BState::blocked_processed: out << "processed,"; break;}

			switch(q->nb->status){
			case BState::processed: case BState::pending: out << "nonblocked"; break; 
			case BState::blocked_pending: case BState::blocked_processed: out << "blocked"; break;}

			if(prune && (q == p || P.contains(keyprio_pair(q)))) out << ",pruned";
			else out << ",notpruned";

			if(q->nb->sleeping) out << ",sleeping";
			else out << ",notsleeping";

			out << '"' << ",texlbl=" << '"' << q->str_latex() << '"';
		}
		break;
	case GTYPE_DOT:
		{
			string fillcolor = "",fontcolor = "",style = "",shape = "";

			(q->nb->ini)?(shape += DOT_source_shape):(shape += DOT_non_source_shape);

			if(q == p) style += ",", style += DOT_current_style + ',';

			if(prune && (q == p || P.contains(keyprio_pair(q))))
				fillcolor += "," + DOT_prune_fillcolor, style += "," + DOT_prune_style, fontcolor = "," + DOT_prune_fontcolor;
			else
				switch(q->nb->status){
				case BState::pending: fillcolor += DOT_pending_fillcolor, style += DOT_nonblocked_style, fontcolor = DOT_non_prune_fontcolor; break;
				case BState::processed: fillcolor += DOT_processed_fillcolor, style += DOT_nonblocked_style, fontcolor = DOT_non_prune_fontcolor; break;
				case BState::blocked_pending: fillcolor += DOT_pending_fillcolor, style += DOT_blocked_style, fontcolor = DOT_non_prune_fontcolor; break;
				case BState::blocked_processed: fillcolor += DOT_processed_fillcolor, style += DOT_blocked_style, fontcolor = DOT_non_prune_fontcolor; break;}

			out << "fontcolor=" << fontcolor << ", " << 
				"fillcolor=" << fillcolor << ", " << 
				"style=" << '"' << style << '"' << ", " << 
				"shape=" << '"' << shape << '"';		
		}
		break;
	}
	out << ']' << ';';

	return out;

}

void print_dot_search_graph(vec_antichain_t& M, non_minimals_t& N, vec_antichain_t& O, bstate_t p, const work_pq& P, bool prune, unsigned ctr_wit, unsigned ctr_pit, string ext) //p will be highlighted
{

	bw_log << "writing uncoverability graph..." << endl;

	string out_fn = net.filename + ".bw-graph.it_" + add_leading_zeros(boost::lexical_cast<string>(ctr_wit),5) + "-" + add_leading_zeros(boost::lexical_cast<string>(ctr_pit),5) + ext + ".dot";

	set<bstate_t> seen;
	stack<bstate_t> S;

	ofstream out(out_fn.c_str());
	if(!out.good())
		throw logic_error(string("cannot write to ") + out_fn);
	out << "digraph BDD {" << endl;

	foreachit(u,M.uv){
		foreach(const bstate_t& q, u->second.M_cref()){

			if(q->nb->sleeping)
				continue;

			S.push(q); seen.insert(q);

			//if(q->nb->status != BState::processed)
			//	continue;

			out << '"' << q->id_str() << '"' << ' ';
			dot_state_out(q,p,P,prune,graph_type,out);
			out << endl;
		}
	}

	foreach(const bstate_t& q, N){

		invariant(!q->nb->sleeping);
		S.push(q); seen.insert(q);

		//if(q->nb->status != BState::processed)
		//	continue;

		out << '"' << q->id_str() << '"' << ' ';
		dot_state_out(q,p,P,prune,graph_type,out);
		out << endl;
	}

	while(!S.empty()){
		bstate_t s = S.top(); S.pop();

		//if(s->nb->status == BState::processed)
		//{
			foreach(bstate_t n, s->nb->suc)
			{
				string to_style;

				//if(n->nb->status == BState::processed)
				//{
					switch(graph_type)
					{
					case GTYPE_TIKZ:
						{
							if(n->nb->src==s->nb->src) to_style = "localpredecessoredge";
							else to_style = "nonlocalpredecessoredge";

							if(n->nb->status == BState::blocked_pending || n->nb->status == BState::blocked_processed) to_style += ",toblocked";
							else to_style += ",tononblocked";

							if(n->nb->sleeping) to_style += ",sleeping";

							out << '"' << s->id_str() << '"' << " -> " << '"' << n->id_str() << '"' << " [style=" << '"' << to_style << '"' << "];" << endl;
						}
						break;
					case GTYPE_DOT:
						{
							if(n->nb->src==s->nb->src) to_style = DOT_local_edge_style;
							else to_style = DOT_non_local_edge_style;

							out << '"' << s->id_str() << '"' << " -> " << '"' << n->id_str() << '"' << " [" << to_style << "];" << endl;
						}
						break;
					}
				//}
			}
		//}
		
		foreach(bstate_t n, s->nb->suc)
			if(seen.insert(n).second)
				S.push(n);
	}

	{
		seen.clear();

		stack<BState const *> C; 
		//foreach(const antichain_t& se, O.uv){
		//	foreach(const bstate_t& n, se.M_cref()){
		foreachit(t, O.uv){
			foreach(const bstate_t& n, t->second.M_cref()){
				C.push(n);
			}
		}

		while(!C.empty()){
			BState const * c = C.top(); C.pop();
			if(!seen.insert(c).second) continue;
			foreach(BState const * n, c->bl->blocked_by){
				C.push(n);

				//if(n->nb->status != BState::processed)
				//	continue;

				switch(graph_type){
				case GTYPE_TIKZ: out << '"' << c->id_str() << '"' << " -> " << '"' << n->id_str() << '"' << " [style=\"blockedby\"];" << endl; break;
				case GTYPE_DOT: out << '"' << c->id_str() << '"' << " -> " << '"' << n->id_str() << '"' << " [" << DOT_blocks_edge_style << "];" << endl; break;}
			}
		}
	}

	out << "}" << endl;
	out.close();
}
