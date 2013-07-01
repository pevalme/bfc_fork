#include "ticketabs.h"

#include <iomanip>

ticketabs::rel_t::rel_t(T v=0):val(v){assert(sizeof(val)==8);}

ticketabs::rel_t::rel_t(unsigned a, unsigned b, unsigned c, unsigned p): pc(p),b1(a),b2(b),b3(c){}

ticketabs::rel_t::T ticketabs::rel_t::C (){ return val & 0xFFFFFFFF00000000; }

ticketabs::rel_t::T ticketabs::rel_t::N (){ return val & 0x00000000FFFFFFFF; }

ticketabs::rel_t::T ticketabs::rel_t::CE(){ return ((C()& 0xFFFF000000000000)) | (0x0000FFFF00000000); } //"quantify out" passive current-state variable (1: keep only active-current, 2: set passive-current to "F")

ticketabs::rel_t::T ticketabs::rel_t::NE(){ return ((N()& 0x00000000FFFF0000)) | (0x000000000000FFFF); }

string ticketabs::rel_t::CH(bool j=1)
{
	stringstream o;
	if(j) o<<setw(D)<<"pc," ; o<<setw(D)<<"b1," <<setw(D)<<"b2," <<setw(D)<<"b3," ;
	if(j) o<<setw(D)<<"pcP,"; o<<setw(D)<<"b1P,"<<setw(D)<<"b2P,"<<setw(D)<<"b3P,";
	return o.str();
}

#define DECO(x) (x==0?"0":(x==1?"1":"*"))

string ticketabs::rel_t::C_str(bool j=1)
{
	stringstream o;
	if(j) o<<setw(D-1)<<pc <<','; o<<setw(D-1)<<DECO(b1) <<','<<setw(D-1)<<DECO(b2) <<','<<setw(D-1)<<DECO(b3) <<',' ;
	if(j) o<<setw(D-1)<<pc1<<','; o<<setw(D-1)<<DECO(b11)<<','<<setw(D-1)<<DECO(b21)<<','<<setw(D-1)<<DECO(b31)<<',';
	return o.str();
}

string ticketabs::rel_t::NH(bool j=1)
{
	stringstream o;
	if(j) o<<setw(D)<<"pc'," ; o<<setw(D)<<"b1'," <<setw(D)<<"b2'," <<setw(D)<<"b3'," ;
	if(j) o<<setw(D)<<"pcP',"; o<<setw(D)<<"b1P',"<<setw(D)<<"b2P',"<<setw(D)<<"b3P',";
	return o.str();
}

string ticketabs::rel_t::N_str(bool j=1)
{
	stringstream o;
	if(j) o<<setw(D-1)<<pcp <<','; o<<setw(D-1)<<DECO(b1p) <<','<<setw(D-1)<<DECO(b2p) <<','<<setw(D-1)<<DECO(b3p) <<',';
	if(j) o<<setw(D-1)<<pc1p<<','; o<<setw(D-1)<<DECO(b11p)<<','<<setw(D-1)<<DECO(b21p)<<','<<setw(D-1)<<DECO(b31p)<<',';
	return o.str();
}

unsigned ticketabs::rel_t::C_numa() const{
	unsigned r=0;
	if((bool)b3) r+=1; if((bool)b2) r+=2; if((bool)b1) r+=4;
	return L*pc + r; 
}

unsigned ticketabs::rel_t::C_nump() const{
	unsigned r=0;
	if((bool)b31) r+=1; if((bool)b21) r+=2; if((bool)b11) r+=4;
	return L*pc1 + r; 
}

unsigned ticketabs::rel_t::N_numa() const{
	unsigned r=0;
	if((bool)b3p) r+=1; if((bool)b2p) r+=2; if((bool)b1p) r+=4;
	return L*pcp + r; 
}

unsigned ticketabs::rel_t::N_nump() const{
	unsigned r=0;
	if((bool)b31p) r+=1; if((bool)b21p) r+=2; if((bool)b11p) r+=4;
	return L*pc1p + r; 
}

void ticketabs::rel_t::test(){
	rel_t Q(0x5555555555555555),C(Q.C()),N(Q.N()),CE(Q.CE()),NE(Q.NE());
	assert(Q.b1 !=0  && Q.b1p != 0 && Q.b11 != 0 && Q.b11p != 0);

	assert(C.val == 0x5555555500000000);
	assert(C.b1 !=0  && C.b1p == 0 && C.b11 != 0 && C.b11p == 0);

	assert(N.val == 0x0000000055555555);
	assert(N.b1 ==0  && N.b1p != 0 && N.b11 == 0 && N.b11p != 0);

	assert(CE.val ==0x5555FFFF00000000);
	assert(CE.b1 !=0  && CE.b1p == 0 && CE.b11 == 0xF && CE.b11p == 0);

	assert(NE.val ==0x000000005555FFFF);
	assert(NE.b1 ==0  && NE.b1p != 0 && NE.b11 == 0 && NE.b11p == 0xF); 
}

ticketabs::ticketabs(unsigned N, unsigned M): NN(N), MMMM(M), l_ini(rel_t(0,0,0,PCE).C_numa()),s_ini(0),l_err(rel_t(0,0,1,PCE).C_numa()),s_err(s_ini + 1),l_end(rel_t(0,1,0,PCE).C_numa()),l_nil(rel_t(0,1,1,PCE).C_numa()),L(l_nil + 1),S(s_err + 1)
{
}

Net ticketabs::initial_net()
{
	Net n;
	n.filename = "TICKETABS";
	n.init.shared = s_ini, n.init.unbounded_locals.insert(l_ini);
	n.target.shared = s_err, n.target.bounded_locals.insert(l_err), n.target.bounded_locals.insert(l_err);
	n.S = S, n.L = L;
	return n;
}

/********************* THIS IS THE MEAT *********************/

//2-thread context (only sound for 2-thread analysis)
#define PRED1N2		((l1!=l11))
#define PRED11N2	((l11!=l1))

//3-thread context
#define PRED1N3		((l1!=l11) && (l1!=l12))
#define PRED11N3	((l11!=l1) && (l11!=l12))

//4-thread context
#define PRED1N4		((l1!=l11) && (l1!=l12) && (l1!=l13))
#define PRED11N4	((l11!=l1) && (l11!=l12) && (l11!=l13))

//5-thread context
#define PRED1N5		((l1!=l11) && (l1!=l12) && (l1!=l13) && (l1!=l14))
#define PRED11N5	((l11!=l1) && (l11!=l12) && (l11!=l13) && (l11!=l14))

//6-thread context
#define PRED1N6		((l1!=l11) && (l1!=l12) && (l1!=l13) && (l1!=l14) && (l1!=l15))
#define PRED11N6	((l11!=l1) && (l11!=l12) && (l11!=l13) && (l11!=l14) && (l11!=l15))

#define PRED2		(t>l1)
#define PRED21		(t>l11)
#define PRED3		(s==l1)
#define PRED31		(s==l11)

static inline bool ticket_algorithm_consistent(int s, int t, int pc, int pc1, int pc2, int pc3, int pc4,  int pc5, int l1, int l11, int l12, int l13, int l14, int l15, bool b1, bool b2, bool b3, bool b11, bool b21, bool b31, int NN){
	switch(NN)
	{
	case 2: return
			b1==PRED1N2 && b11==PRED11N2 &&	
			b2==PRED2 && b21==PRED21 &&	
			b3==PRED3 && b31==PRED31;
	case 3: return
			b1==PRED1N3 && b11==PRED11N3 &&	
			b2==PRED2 && b21==PRED21 &&	
			b3==PRED3 && b31==PRED31;
	case 4: return
			b1==PRED1N4 && b11==PRED11N4 &&	
			b2==PRED2 && b21==PRED21 &&	
			b3==PRED3 && b31==PRED31;
	case 5: return
			b1==PRED1N5 && b11==PRED11N5 &&	
			b2==PRED2 && b21==PRED21 &&	
			b3==PRED3 && b31==PRED31;
	case 6: return
			b1==PRED1N6 && b11==PRED11N6 &&	
			b2==PRED2 && b21==PRED21 &&	
			b3==PRED3 && b31==PRED31;
	default:assert(0);
	}
}

std::pair<Net::adj_t,Net::adj_t> ticketabs::get_adjacency_lists(const BState& b)
{
	Net::adj_t a; //sending inconsistent passive local state to l_nil-local

	set<rel_t::T> INIs,ERRs,CONs,RELs, REL_no1s, REL_no2s, REL_monvios, RSTs;



#define loopvv(e,l,h) for(e = l; e <= (h); ++e)
#define loopb(e) for(e = 0; e <= 1; ++e)
#define loopb(e) for(e = 0; e <= 1; ++e)

#define CON_VAL	\
	t		<<setw(rel_t::D)<< s	<<setw(rel_t::D)<< \
	T.pc	<<setw(rel_t::D)<< l1	<<setw(rel_t::D)<< \
	T.pc1	<<setw(rel_t::D)<< l11	<<setw(rel_t::D)<< \
	pc2		<<setw(rel_t::D)<< l12	<<setw(rel_t::D)<< \
	pc3		<<setw(rel_t::D)<< l13	<<setw(rel_t::D)<< \
	pc4		<<setw(rel_t::D)<< l14	<<setw(rel_t::D)<< \
	pc5		<<setw(rel_t::D)<< l15	<<setw(rel_t::D)<< \
	tp		<<setw(rel_t::D)<< sp	<<setw(rel_t::D)<< \
	T.pcp	<<setw(rel_t::D)<< l1p	<<setw(rel_t::D)<< \
	T.pc1p	<<setw(rel_t::D)<< l11p	<<setw(rel_t::D)<< \
	pc2p	<<setw(rel_t::D)<< l12p	<<setw(rel_t::D)<< \
	pc3p	<<setw(rel_t::D)<< l13p	<<setw(rel_t::D)<< \
	pc4p	<<setw(rel_t::D)<< l14p	<<setw(rel_t::D)<< \
	pc5p	<<setw(rel_t::D)<< l15p	<<setw(rel_t::D)

#define CON_VAL_H \
	"t"		<<setw(rel_t::D)<< "s"		<<setw(rel_t::D)<< \
	"pc"	<<setw(rel_t::D)<< "l1"		<<setw(rel_t::D)<< \
	"pc1"	<<setw(rel_t::D)<< "l11"	<<setw(rel_t::D)<< \
	"pc2"	<<setw(rel_t::D)<< "l12"	<<setw(rel_t::D)<< \
	"pc3"	<<setw(rel_t::D)<< "l13"	<<setw(rel_t::D)<< \
	"pc4"	<<setw(rel_t::D)<< "l14"	<<setw(rel_t::D)<< \
	"pc5"	<<setw(rel_t::D)<< "l15"	<<setw(rel_t::D)<< \
	"tp"	<<setw(rel_t::D)<< "sp"		<<setw(rel_t::D)<< \
	"pcp"	<<setw(rel_t::D)<< "l1p"	<<setw(rel_t::D)<< \
	"pc1p"	<<setw(rel_t::D)<< "l11p"	<<setw(rel_t::D)<< \
	"pc2p"	<<setw(rel_t::D)<< "l12p"	<<setw(rel_t::D)<< \
	"pc3p"	<<setw(rel_t::D)<< "l13p"	<<setw(rel_t::D)<< \
	"pc4p"	<<setw(rel_t::D)<< "l14p"	<<setw(rel_t::D)<< \
	"pc5p"	<<setw(rel_t::D)<< "l15p"	<<setw(rel_t::D)

#define SEP				" // "
	cout << "HEA: " << rel_t::CH() << rel_t::NH() << SEP << CON_VAL_H << endl;

	int pc2=PC1,pc2p=PC1,pc3=PC1,pc3p=PC1,pc4=PC1,pc4p=PC1,pc5=PC1,pc5p=PC1;
	int s,sp,t,tp,l1,l1p,l11,l11p,l12=0,l12p=0,l13=0,l13p=0,l14=0,l14p=0,l15=0,l15p=0;
	rel_t T;
	{
#define PRINTT	T.C_str() << T.N_str() << SEP << CON_VAL

#define LOOP_PCTR		loopvv(T.pc,PC1,PC3)loopvv(T.pc1,PC1,PC3)\
	loopvv(pc2,PC1,NN < 3?PC1:PCE)\
	loopvv(pc3,PC1,NN < 4?PC1:PCE)\
	loopvv(pc4,PC1,NN < 5?PC1:PCE)\
	loopvv(pc5,PC1,NN < 6?PC1:PCE)
#define LOOP_VARS		loopvv(s,0,MMMM)loopvv(t,0,MMMM)loopvv(l1,0,MMMM)\
	loopvv(l11,0,MMMM)\
	loopvv(l12,0,NN < 3?0:MMMM)\
	loopvv(l13,0,NN < 4?0:MMMM)\
	loopvv(l14,0,NN < 5?0:MMMM)\
	loopvv(l15,0,NN < 6?0:MMMM)
#define LOOP_BOOL		loopb(T.b1)loopb(T.b2)loopb(T.b3)loopb(T.b11)loopb(T.b21)loopb(T.b31)
#define INI				(s==1) && (t==1) && (T.pc == PC1) && (l1==0) && (T.pc1 == PC1) && (l11==0) && \
	(NN < 3 || pc2 == PC1) && (NN < 3 || l12==0) && \
	(NN < 4 || pc3 == PC1) && (NN < 4 || l13==0) && \
	(NN < 5 || pc4 == PC1) && (NN < 5 || l14==0) && \
	(NN < 6 || pc5 == PC1) && (NN < 6 || l15==0)
#define ERR				(T.pc == PC3) && (T.pc1 == PC3) //&& (NN < 3 || pc2 == PC3) && (NN < 4 || pc3 == PC3)

		T.val = 0;
		LOOP_PCTR
		{
			cout << "pcs:" << T.pc << T.pc1 << pc2 << pc3 << pc4 << pc5 << endl;
			//#pragma omp parallel for private(T) shared(a,CONs,INIs,ERRs,s,t,pc2,pc3,pc4,l1,l11,l12,l13,l14,NN)
			LOOP_VARS 
			{
				if(!INI && !ERR)
					continue;

				LOOP_BOOL
				{
					if(!ticket_algorithm_consistent(s,t,T.pc,T.pc1,pc2,pc3,pc4,pc5,l1,l11,l12,l13,l14,l15,T.b1,T.b2,T.b3,T.b11,T.b21,T.b31,NN))
						continue;

					if(CONs.insert(T.C()).second){
						//cout << "CON: " << PRINTT << endl;
					}
					if(INI && INIs.insert(T.C()).second){//if(con && INI && INIs.insert(T.C()).second){
						a[Thread_State(s_ini,l_ini)][Thread_State(s_ini,T.C_numa())][l_ini].insert(T.C_nump());
						//cout << "INI: " << PRINTT << endl; 
					}
					if(ERR && ERRs.insert(T.C()).second){//if(con && ERR && ERRs.insert(T.C()).second){
						a[Thread_State(s_ini,T.C_numa())][Thread_State(s_err,l_err)][T.C_nump()].insert(l_err); //go (together with all passive threads) to the error state
						//cout << "ERR: " << PRINTT << endl; 
					}
				}
			}
		}

		cout << "next" << endl;

#define LOOP_PCTR		loopvv(T.pc,PC1,PC3)loopvv(T.pcp,T.pc+1,T.pc+1)loopvv(T.pc1,PC1,PC3)loopvv(T.pc1p,T.pc1,T.pc1)\
	loopvv(pc2,PC1,NN < 3?PC1:PCE)loopvv(pc2p,pc2,pc2)\
	loopvv(pc3,PC1,NN < 4?PC1:PCE)loopvv(pc3p,pc3,pc3)\
	loopvv(pc4,PC1,NN < 5?PC1:PCE)loopvv(pc4p,pc4,pc4)\
	loopvv(pc5,PC1,NN < 6?PC1:PCE)loopvv(pc5p,pc5,pc5)
#define LOOP_VARS		loopvv(s,0,MMMM)loopvv(sp,0,MMMM)loopvv(t,0,MMMM)loopvv(tp,0,MMMM) \
	loopvv(l1,0,MMMM)loopvv(l1p,0,MMMM) \
	loopvv(l11,0,MMMM)loopvv(l11p,l11,l11) \
	loopvv(l12,0,NN<3?0:MMMM)loopvv(l12p,l12,l12) \
	loopvv(l13,0,NN<4?0:MMMM)loopvv(l13p,l13,l13) \
	loopvv(l14,0,NN<5?0:MMMM)loopvv(l14p,l14,l14) \
	loopvv(l15,0,NN<6?0:MMMM)loopvv(l15p,l15,l15)

#define LOOP_BOOL		loopb(T.b1)loopb(T.b1p)loopb(T.b2)loopb(T.b2p)loopb(T.b3)loopb(T.b3p)loopb(T.b11)loopb(T.b11p)loopb(T.b21)loopb(T.b21p)loopb(T.b31)loopb(T.b31p)
#define PCUP(p)			(T.pc == p) && (T.pcp == p+1) && (T.pc1p == T.pc1) && (NN < 3 || pc2p == pc2) && (NN < 4 || pc3p == pc3) && (NN < 5 || pc4p == pc4) && (NN < 6 || pc5p == pc5)
#define PASSIVE			(l11p==l11) && (NN < 3 || l12p==l12) && (NN < 4 || l13p==l13) && (NN < 5 || l14p==l14) && (NN < 6 || l15p==l15)
#define OP1				PCUP(PC1)	&& PASSIVE	&& (sp==s)			&& (tp==t + 1)	&& (l1p==t)		//PC1: l := t++;
#define OP2				PCUP(PC2)	&& PASSIVE	&& (sp==s && s==l1)	&& (tp==t)		&& (l1p==l1)	//PC2: assume(s==m)
#define OP3				PCUP(PC3)	&& PASSIVE	&& (sp==s+1)		&& (tp==t)		&& (l1p==l1)	//PC3: s := s+1;

		//******************************************************************************************************

		T.val = 0;
		LOOP_PCTR 
		{
			cout << "pcs:" << T.pc << T.pc1 << pc2 << pc3 << pc4 << pc5 << endl;

			assert(PCUP(PC1) || PCUP(PC2) || PCUP(PC3));

			//#pragma omp parallel for
#define LOOP_VARS		loopvv(s,0,MMMM)loopvv(sp,(T.pc==PC1||T.pc==PC2)?s:(T.pc==PC3?s+1:(0)),(T.pc==PC1||T.pc==PC2)?s:(T.pc==PC3?s+1:(MMMM)))loopvv(t,0,MMMM)loopvv(tp,T.pc==PC1?t+1:((T.pc==PC2||T.pc==PC3)?t:(0)),T.pc==PC1?t+1:((T.pc==PC2||T.pc==PC3)?t:(MMMM))) \
	loopvv(l1,T.pc==PC2?s:(0),T.pc==PC2?s:(MMMM))loopvv(l1p,T.pc==PC1?t:((T.pc==PC2||T.pc==PC3)?l1:(0)),T.pc==PC1?t:((T.pc==PC2||T.pc==PC3)?l1:(MMMM))) \
	loopvv(l11,0,MMMM)loopvv(l11p,l11,l11) \
	loopvv(l12,0,NN<3?0:MMMM)loopvv(l12p,l12,l12) \
	loopvv(l13,0,NN<4?0:MMMM)loopvv(l13p,l13,l13) \
	loopvv(l14,0,NN<5?0:MMMM)loopvv(l14p,l14,l14) \
	loopvv(l15,0,NN<6?0:MMMM)loopvv(l15p,l15,l15)
			LOOP_VARS
			{

				assert(PASSIVE);

				assert(OP1 || OP2 || OP3);

				LOOP_BOOL
				{

					bool con = ticket_algorithm_consistent(s,t,T.pc,T.pc1,pc2,pc3,pc4,pc5,l1,l11,l12,l13,l14,l15,T.b1,T.b2,T.b3,T.b11,T.b21,T.b31,NN) && ticket_algorithm_consistent(sp,tp,T.pcp,T.pc1p,pc2p,pc3p,pc4p,pc5p,l1p,l11p,l12p,l13p,l14p,l15p,T.b1p,T.b2p,T.b3p,T.b11p,T.b21p,T.b31p,NN);

					if(con && RELs.insert(T.C() | T.N()).second){ //if(con && (OP1 || OP2 || OP3) && RELs.insert(T.C() | T.N()).second){
						REL_no1s.insert(T.CE()	| T.NE());
						REL_no2s.insert(T.C()	| T.NE());

						Thread_State	A(s_ini,T.C_numa()), AN(T.pc == PC3?Thread_State(s_ini,l_end):Thread_State(s_ini,T.N_numa()));
						unsigned		P(T.C_nump()), PN(T.N_nump());
						a[A][AN][P].insert(PN); //passive transition

						cout << "TRA: " << PRINTT << endl;
					}
				}
			}
		}
	}

	{
		T.val = 0;
		LOOP_PCTR LOOP_BOOL
		{
			if(REL_no2s.find(T. C() | T.NE()) == REL_no2s.end() && REL_no1s.find(T.CE() | T.NE()) != REL_no1s.end()){ //not exists ... && exists ...
				REL_monvios.insert(T. C() | T. N());

				Thread_State A(s_ini,T.C_numa()), AN(T.pc == PC3?Thread_State(s_ini,l_end):Thread_State(s_ini,T.N_numa()));
				unsigned P(T.C_nump());

				a[A][AN][P].insert(l_nil); //passive transition
				if(RSTs.insert(T. C() | T.NE()).second)
				{
					//cout << "RST: " << PRINTT << endl;
				}
			}
		}
	}
	{
		cout 
			<< "INI" << INIs.size() << endl
			<< "ERR" << ERRs.size() << endl
			<< "CON" << CONs.size() << endl
			<< "REL" << RELs.size() << endl
			<< "RST" << RSTs.size() << endl
			;

		/*
		//Abstract net
		Net n_move_inconsistent(S,L,ini,err,a,a,true);
		Net n_ignore_non_mon(S,L,ini,err,b,b,true);

		stringstream Nethead;
		Nethead << "#shared states" << endl;
		Nethead << "#" << "0" << " -> " << s_ini << endl;
		Nethead << "#" << "1" << " -> " << s_ini+1 << endl;
		Nethead << "#local states" << endl;
		unsigned pc,b1,b2,b3;
		loopvv(pc,PC1,PC3)loopb(b1)loopb(b2)loopb(b3) Nethead << "#" << b1 << b2 << b3 << "/" << pc << " -> " << rel_t(b1,b2,b3,pc).C_numa() << endl;
		Nethead << "#auxiliary local states" << endl;
		Nethead << "#" << "ini" << " -> " << l_ini << endl;
		Nethead << "#" << "err" << " -> " << l_err << endl;
		Nethead << "#" << "end" << " -> " << l_end << endl;
		Nethead << "#" << "nil" << " -> " << l_nil << endl;
		Nethead << endl;		
		ofstream NETf(base + "net_mocinc.tts"); NETf << Nethead.str() << n_move_inconsistent << endl;
		ofstream NETfnm(base + "net_ignmon.tts"); NETfnm << Nethead.str() << n_ignore_non_mon << endl;
		*/

	}
	cout << endl;





	assert(0);
	Net::adj_t adjacency_list_inv;
	return make_pair(a, adjacency_list_inv);
}
