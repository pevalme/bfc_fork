#include "ticketabs.h"

#include <iomanip>
#include <fstream>
#include "boost/lexical_cast.hpp"

using namespace std;

ticketabs::rel_t::rel_t(T v):val(v){assert(sizeof(val)==8);}

ticketabs::rel_t::rel_t(unsigned a, unsigned b, unsigned c, unsigned p): pc(p),b1(a),b2(b),b3(c){}

ticketabs::rel_t::rel_t(const unsigned l, bool P)
{
	val = 0x3333333333333333;
	if(!P)
	{
		pc = l / 8;
		assert(pc <= PC3);
		b1 = (bool)(l&4);
		b2 = (bool)(l&2);
		b3 = (bool)(l&1);
	}
	else
	{
		pcp = l / 8;
		assert(pcp <= PC3);
		b1p = (bool)(l&4);
		b2p = (bool)(l&2);
		b3p = (bool)(l&1);
	}
}

bool ticketabs::rel_t::operator < (const rel_t& r) const{ return val < r.val; }

ticketabs::rel_t::T ticketabs::rel_t::C (){ return val & 0xFFFFFFFF00000000; }

ticketabs::rel_t::T ticketabs::rel_t::N (){ return val & 0x00000000FFFFFFFF; }

ticketabs::rel_t::T ticketabs::rel_t::CE(){ return ((C()& 0xFFFF000000000000)) | (0x0000FFFF00000000); } //"quantify out" passive current-state variable (1: keep only active-current, 2: set passive-current to "F")

ticketabs::rel_t::T ticketabs::rel_t::NE(){ return ((N()& 0x00000000FFFF0000)) | (0x000000000000FFFF); }

string ticketabs::rel_t::CH(bool j=1)
{
	stringstream o;
	if(j) o<<setw(D)<<"p " ; o<<setw(D)<<"b1 " <<setw(D)<<"b2 " <<setw(D)<<"b3 " ;
	if(j) o<<setw(D)<<"P "; o<<setw(D)<<"B1 "<<setw(D)<<"B2 "<<setw(D)<<"B3 ";
	return o.str();
}

#define DECO(x) (x==0?"0":(x==1?"1":"*"))

string ticketabs::rel_t::C_str(bool j=1)
{
	stringstream o;
	if(j) o<<setw(D-1)<<pc <<' '; o<<setw(D-1)<<DECO(b1) <<' '<<setw(D-1)<<DECO(b2) <<' '<<setw(D-1)<<DECO(b3) <<' ' ;
	if(j) o<<setw(D-1)<<pc1<<' '; o<<setw(D-1)<<DECO(b11)<<' '<<setw(D-1)<<DECO(b21)<<' '<<setw(D-1)<<DECO(b31)<<' ';
	return o.str();
}

string ticketabs::rel_t::NH(bool j=1)
{
	stringstream o;
	if(j) o<<setw(D)<<"p' " ; o<<setw(D)<<"b1' " <<setw(D)<<"b2' " <<setw(D)<<"b3' " ;
	if(j) o<<setw(D)<<"P' "; o<<setw(D)<<"B1' "<<setw(D)<<"B2' "<<setw(D)<<"B3' ";
	return o.str();
}

string ticketabs::rel_t::N_str(bool j=1)
{
	stringstream o;
	if(j) o<<setw(D-1)<<pcp <<' '; o<<setw(D-1)<<DECO(b1p) <<' '<<setw(D-1)<<DECO(b2p) <<' '<<setw(D-1)<<DECO(b3p) <<' ';
	if(j) o<<setw(D-1)<<pc1p<<' '; o<<setw(D-1)<<DECO(b11p)<<' '<<setw(D-1)<<DECO(b21p)<<' '<<setw(D-1)<<DECO(b31p)<<' ';
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
	rel_t Q,C,N,CE,NE;
	Q.val = 0x5555555555555555;
	C.val = Q.C(), N.val = Q.N(), CE.val = Q.CE(), NE.val = Q.NE();
	
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

ticketabs::ticketabs(unsigned N, unsigned M): N(N), M(M), l_ini(rel_t(0,0,0,PCE).C_numa()),s_ini(0),l_err(rel_t(0,0,1,PCE).C_numa()),s_err(s_ini + 1),l_end(rel_t(0,1,0,PCE).C_numa()),l_nil(rel_t(0,1,1,PCE).C_numa()),L(l_nil + 1),S(s_err + 1)
{
}

void ticketabs::initial_net(Net& n)
{
	n.filename = "TICKETABS";

	//initial state
	n.init.shared = s_ini, n.init.unbounded_locals.insert(l_ini);

	//target state
	//safe:
	n.target.shared = s_err, n.target.bounded_locals.insert(l_err), n.target.bounded_locals.insert(l_err);

	//unsafe:
	//n.target.shared = 0, n.target.bounded_locals.insert(2);
	//n.target.shared = 0, n.target.bounded_locals.insert(15);

	//n.target.shared = s_err, n.target.bounded_locals.insert(l_err);

	//dimension
	n.S = n.S_input = S, n.L = n.L_input = L;

	//consistency
	n.core_shared = n.get_core_shared(true);
}

/********************* THIS IS THE MEAT *********************/

#define PC_PRGS\
	n.reduce_log << "pcs: ";\
	for(unsigned P = 0; P <= 1; ++P)\
	for(unsigned a = 0; a < N; ++a) {\
	unsigned x = PC_(a,P);\
	n.reduce_log << x << (a==N-1 && P==0?"-":""); } n.reduce_log << endl\

#define CON_VAL \
	for(unsigned P = 0; P <= 1; ++P)\
	for(unsigned b = 0; b < N; ++b) {\
	if(b == 0) cerr << T_(P) << setw(rel_t::D)<< S_(P) << setw(rel_t::D);\
	cerr << PC_(b,P) << setw(rel_t::D) << L_(b,P) << setw(rel_t::D); }

#define CON_VAL_H \
	for(unsigned P = 0; P <= 1; ++P)\
	for(unsigned b = 0; b < N; ++b) {\
	if(b == 0) cerr << T_str(P) << setw(rel_t::D)<< S_str(P) << setw(rel_t::D);\
	cerr << PC_str(b,P) << setw(rel_t::D) << L_str(b,P) << setw(rel_t::D); }

#define SEP		" // "
#define PRINTT	cerr << T.C_str() << T.N_str() << SEP; CON_VAL;

#define PC(a)  pc[a-2]
#define PCP(a) pcp[a-2]
#define PC_(a,P) (\
	a==0?(P?T.pcp: T.pc ):(\
	a==1?(P?T.pc1p:T.pc1):(\
	P?PCP(a):PC(a))))
#define PC_str(a,P) ((string)"p" + (boost::lexical_cast<string>(a)) + (P?"'":""))

#define S_(P) (P?sp:s)
#define T_(P) (P?tp:t)
#define S_str(P) ((string)"s" + (P?"'":""))
#define T_str(P) ((string)"t" + (P?"'":""))

#define LC(a) l[a]
#define LP(a) lp[a]
#define L_(a,P) (P?LP(a):LC(a))
#define L_str(a,P) ((string)"m" + (boost::lexical_cast<string>(a)) + (P?"'":""))

#define BB(a,c,P) (\
	a==0 && c == 0?(P?T.b1p: T.b1 ):(\
	a==0 && c == 1?(P?T.b2p: T.b2 ):(\
	a==0 && c == 2?(P?T.b3p: T.b3 ):(\
	a==1 && c == 0?(P?T.b11p:T.b11):(\
	a==1 && c == 1?(P?T.b21p:T.b21):(\
	a==1 && c == 2?(P?T.b31p:T.b31):(\
	P?bp[a-2][c]:b[a-2][c])))))))

#define LOOP(e,l,h) for((e) = (l); (e) <= (h); ++(e))

#define bLOOP(e) LOOP(e,0,1)

#define LOOP_PCTR \
	LOOP(T.pc,PC1,N<1?PC1:PC3)\
	LOOP(T.pc1,PC1,N<2?PC1:PC3)\
	LOOP(PC(2),PC1,N<3?PC1:PCE)\
	LOOP(PC(3),PC1,N<4?PC1:PCE)\
	LOOP(PC(4),PC1,N<5?PC1:PCE)\
	LOOP(PC(5),PC1,N<6?PC1:PCE)

#define LOOP_VARS \
	LOOP(s,0,M)\
	LOOP(t,0,M)\
	LOOP(LC(0),0,N<1?0:M)\
	LOOP(LC(1),0,N<2?0:M)\
	LOOP(LC(2),0,N<3?0:M)\
	LOOP(LC(3),0,N<4?0:M)\
	LOOP(LC(4),0,N<5?0:M)\
	LOOP(LC(5),0,N<6?0:M)

#define LOOP_BOOL \
	bLOOP(T.b1 )\
	bLOOP(T.b2 )\
	bLOOP(T.b3 )\
	bLOOP(T.b11)\
	bLOOP(T.b21)\
	bLOOP(T.b31)

#define LOOP_BOOL_N \
	bLOOP(T.b1p )\
	bLOOP(T.b2p )\
	bLOOP(T.b3p )\
	bLOOP(T.b11p)\
	bLOOP(T.b21p)\
	bLOOP(T.b31p)

#define LOOP_PCTR_CN\
	LOOP_PCTR\
	LOOP(T.pcp,T.pc+1,T.pc+1)\
	LOOP(T.pc1p,T.pc1,T.pc1)\
	LOOP(PCP(2),PC(2),PC(2))\
	LOOP(PCP(3),PC(3),PC(3))\
	LOOP(PCP(4),PC(4),PC(4))\
	LOOP(PCP(5),PC(5),PC(5))

#define LOOP_VARS_CN\
	LOOP(s,0,M)\
	LOOP(sp,(T.pc==PC1||T.pc==PC2)?s:(T.pc==PC3?s+1:(0)),(T.pc==PC1||T.pc==PC2)?s:(T.pc==PC3?s+1:(M)))\
	LOOP(t,0,M)\
	LOOP(tp,T.pc==PC1?t+1:((T.pc==PC2||T.pc==PC3)?t:(0)),T.pc==PC1?t+1:((T.pc==PC2||T.pc==PC3)?t:(M)))\
	LOOP(LC(0),T.pc==PC2?s:(0),T.pc==PC2?s:(M))\
	LOOP(LP(0),T.pc==PC1?t:((T.pc==PC2||T.pc==PC3)?LC(0):(0)),T.pc==PC1?t:((T.pc==PC2||T.pc==PC3)?LC(0):(M)))\
	LOOP(LC(1),0,M)\
	LOOP(LP(1),LC(1),LC(1))\
	LOOP(LC(2),0,N<3?0:M)\
	LOOP(LP(2),LC(2),LC(2))\
	LOOP(LC(3),0,N<4?0:M)\
	LOOP(LP(3),LC(3),LC(3))\
	LOOP(LC(4),0,N<5?0:M)\
	LOOP(LP(4),LC(4),LC(4))\
	LOOP(LC(5),0,N<6?0:M)\
	LOOP(LP(5),LC(5),LC(5))

#define PCUP(p)\
	(\
	(N<1||T.pc   == p && T.pcp  == p+1) && \
	(N<2||T.pc1p == T.pc1) && \
	(N<3||PCP(2) == PC(2)) && \
	(N<4||PCP(3) == PC(3)) && \
	(N<5||PCP(4) == PC(4)) && \
	(N<6||PCP(5) == PC(5)) && \
	1)

#define PASSIVE	((N<2||LP(1)==LC(1)) && (N<3||LP(2)==LC(2)) && (N<4||LP(3)==LC(3)) && (N<5||LP(4)==LC(4)) && (N<6||LP(5)==LC(5)))
#define OP1		(PCUP(PC1)	&& PASSIVE	&& (sp==s)				&& (tp==t + 1)	&& (LP(0)==t))		//PC1: l := t++;
#define OP2		(PCUP(PC2)	&& PASSIVE	&& (sp==s && s==LC(0))	&& (tp==t)		&& (LP(0)==LC(0)))	//PC2: assume(s==m)
#define OP3		(PCUP(PC3)	&& PASSIVE	&& (sp==s+1)			&& (tp==t)		&& (LP(0)==LC(0)))	//PC3: s := s+1;

#define ERR_PC	\
	(\
	(T.pc  == PC3) && \
	(T.pc1 == PC3) && \
	1)

#define INI_PC \
	(\
	(N<1||T.pc  == PC1) && \
	(N<2||T.pc1 == PC1) && \
	(N<3||PC(2) == PC1) && \
	(N<4||PC(3) == PC1) && \
	(N<5||PC(4) == PC1) && \
	(N<6||PC(5) == PC1) && \
	1)

#define INI \
	(\
	(s==1) && (t==1) && \
	(N<1||LC(0)==0) && \
	(N<2||LC(1)==0) && \
	(N<3||LC(2)==0) && \
	(N<4||LC(3)==0) && \
	(N<5||LC(4)==0) && \
	(N<6||LC(5)==0) && \
	1)

unsigned s,sp;
unsigned t,tp;
unsigned pc[4],pcp[4]; //pc of 3rd to 6th passive thread; pcs of 1st and 2nd thread are stored in T
unsigned l[6],lp[6];
unsigned b[4][3],bp[4][3];

ticketabs::rel_t T; 

#define PRED1N2		((LC(0)!=LC(1)))
#define PRED11N2	((LC(1)!=LC(0)))

#define PRED1N3		(LC(0)!=LC(1) && LC(0)!=LC(2))
#define PRED11N3	(LC(1)!=LC(0) && LC(1)!=LC(2))

#define PRED2		(t>LC(0))
#define PRED21		(t>LC(1))
#define PRED3		(s==LC(0))
#define PRED31		(s==LC(1))

bool ticketabs::PRED(unsigned a, unsigned c, bool P) const
{
	switch(c)
	{
	case 0: for(unsigned p = 0; p < N; ++p)
				if(p != a && !(L_(a,P)!=L_(p,P)))
					return false; 
		return true; 

		//case 1:	return (L_(a,P) < T_(P)); //predicate: l < t
	case 1: for(unsigned p = 0; p < N; ++p) //predicate: l < t && lP < t
				if(!(L_(p,P) < T_(P)))
					return false; 
		return true; 

	case 2:	return (L_(a,P) == S_(P));

	default: throw;
	}
}

bool ticketabs::consistent(unsigned m, bool P) const
{
	for(unsigned c = 0; c <= 2; ++c) //3 predicates
		for(unsigned a = 0; a <= m; ++a) //m threads
			if(BB(a,c,P)!=PRED(a,c,P))
				return false;
	return true;
}

void ticketabs::test(Net& n)
{
}

//state.bounded_locals.empty() is equivalent to constraining all transitoin with "false" and hence no transition is generated
void ticketabs::update_transitions(Net& n)
{

	n.reduce_log << "M (data-range): 0..." << M << endl;
	n.reduce_log << "N (abstract threads): " << N << endl;

	n.adjacency_list.clear(), n.net_changed = true;
	Net::adj_t adjacency_list_ignmon; //ignoring non-monotonicit

	set<rel_t::T> INIs,ERRs,RELs, REL_no1s, REL_no2s,REL_monvios, RSTs;

	n.reduce_log << "HEA: " << rel_t::CH() << rel_t::NH() << SEP; CON_VAL_H; n.reduce_log << endl;

	//abstract initial and error states

	//always generate transition to error and initial states
	T.val = 0; //initialize
	{
		LOOP_PCTR 
		{
			//PC_PRGS;
			LOOP_VARS LOOP_BOOL
			{
				if(!ERR_PC) continue;
				if(!consistent(1,false)) continue;
				if(!ERRs.insert(T.C()).second) continue;

				n.adjacency_list[Thread_State(s_ini,T.C_numa())][Thread_State(s_err,l_err)][T.C_nump()].insert(l_err); //go (together with all passive threads) to the error state
				/*n.reduce_log << "ERR: "; PRINTT; n.reduce_log << endl;*/
			}
		}
		n.reduce_log << "ERR" << ERRs.size() << endl;
	}
	{
		LOOP_PCTR 
		{
			PC_PRGS;
			LOOP_VARS LOOP_BOOL
			{
				if(!(INI_PC && INI)) continue;
				if(!consistent(1,false)) continue;
				if(!INIs.insert(T.C()).second) continue;

				n.adjacency_list[Thread_State(s_ini,l_ini)][Thread_State(s_ini,T.C_numa())][l_ini].insert(T.C_nump());
				/*n.reduce_log << "INI: "; PRINTT; n.reduce_log << endl;*/ 
			}
		}
		n.reduce_log << "INI" << INIs.size() << endl;
	}

	//of the special values only l_end needs to be treated below
	{
		//process transitions from non-initial states here
		T.val = 0;
		LOOP_PCTR_CN
		{
			PC_PRGS;
			LOOP_BOOL LOOP_BOOL_N LOOP_VARS_CN
			{
				assert(PCUP(PC1)||PCUP(PC2)||PCUP(PC3) && PASSIVE && (OP1 || OP2 || OP3));

				if(!consistent(1,false) || !consistent(1,true))
					continue;

				assert(T.b2 == T.b21);
				assert(T.b2p == T.b21p);

				if(RELs.insert(T.C() | T.N()).second){
					REL_no1s.insert(T.CE()	| T.NE());
					REL_no2s.insert(T.C()	| T.NE());

					Thread_State	A(s_ini,T.C_numa()), AN(T.pc == PC3?Thread_State(s_ini,l_end):Thread_State(s_ini,T.N_numa()));
					unsigned		P(T.C_nump()), PN(T.N_nump());

					assert(A.local < n.L);
					assert(AN.local < n.L);
					assert(P < n.L);
					assert(PN < n.L);
					assert(A.shared < n.S);
					assert(AN.shared < n.S);

					n.adjacency_list[A][AN][P].insert(PN); //passive transition
					//n.reduce_log << "TRA: "; PRINTT; n.reduce_log << endl;
				}
			}
		}

		//monotonize relation
		adjacency_list_ignmon = n.adjacency_list;

		T.val = 0;
		LOOP_PCTR_CN LOOP_BOOL LOOP_BOOL_N
		{
			if(REL_no2s.find(T. C() | T.NE()) == REL_no2s.end() && REL_no1s.find(T.CE() | T.NE()) != REL_no1s.end()){ //not exists ... && exists ...
				REL_monvios.insert(T. C() | T. N());

				Thread_State A(s_ini,T.C_numa()), AN(PC_(0,false) == PC3?Thread_State(s_ini,l_end):Thread_State(s_ini,T.N_numa()));
				//Thread_State A(s_ini,T.C_numa()), AN(PC(0) == PC3?Thread_State(s_ini,l_end):Thread_State(s_ini,T.N_numa()));
				unsigned P(T.C_nump());

				assert(A.local < n.L);
				assert(AN.local < n.L);
				assert(P < n.L);
				assert(A.shared < n.S);
				assert(AN.shared < n.S);

				n.adjacency_list[A][AN][P].insert(l_nil); //passive transition
				if(RSTs.insert(T. C() | T.NE()).second)
				{ /*n.reduce_log << "RST: "; PRINTT; n.reduce_log << endl;*/ }
			}
		}

		//final output
		n.reduce_log 
			<< "REL" << RELs.size() << endl
			<< "RST" << RSTs.size() << endl
			<< endl;
	}

	string base = "ticketabs_N" + boost::lexical_cast<string>(N) + "M" + boost::lexical_cast<string>(M) + "_";

	ofstream f;

	f.close(),f.open(base+"num.csv");
	f << "INI" << INIs.size() << endl
		<< "ERR" << ERRs.size() << endl
		<< "REL" << RELs.size() << endl
		<< "RST" << RSTs.size() << endl
		;

	//f.close(),f.open(base+"ERR.csv");
	//f << rel_t::CH(0) << ",f" << endl;
	//for(auto a:ERRs) f << rel_t(a).C_str(0) << ",1" << endl;

	//f.close(),f.open(base+"INI.csv");
	//f << rel_t::CH(0) << ",f" << endl;
	//for(auto a:INIs) f << rel_t(a).C_str(0) << ",1" << endl;

	//f.close(),f.open(base+"REL.csv");
	//f << rel_t::CH(0) << rel_t::NH(0) << ",f" << endl;
	//for(auto a:RELs) if(rel_t(a).pc==PC1) f << rel_t(a).C_str(0) << rel_t(a).N_str(0) << ",1" << endl;

	//f.close(),f.open(base+"RST.csv");
	//f << rel_t::CH(0) << rel_t::NH(0) << ",f" << endl;
	//for(auto a:RSTs) if(rel_t(a).pc==PC1) f << rel_t(a).C_str(0) << rel_t(a).N_str(0) << ",1" << endl;

	//Abstract net
	OState ini(s_ini); ini.unbounded_locals.insert(l_ini);

	BState err(s_err); err.bounded_locals.insert(l_err), err.bounded_locals.insert(l_err);
	Net n_move_inconsistent(S,L,ini,err,n.adjacency_list,true);
	Net n_ignore_non_mon(S,L,ini,err,adjacency_list_ignmon,true);

	stringstream Nethead;
	Nethead << "#shared states" << endl;
	Nethead << "#" << "0" << " -> " << s_ini << endl;
	Nethead << "#" << "1" << " -> " << s_ini+1 << endl;
	Nethead << "#local states" << endl;
	unsigned pc,b1,b2,b3;
	LOOP(pc,PC1,PC3)bLOOP(b1)bLOOP(b2)bLOOP(b3) Nethead << "#" << b1 << b2 << b3 << "/" << pc << " -> " << rel_t(b1,b2,b3,pc).C_numa() << endl;
	Nethead << "#auxiliary local states" << endl;
	Nethead << "#" << "ini" << " -> " << l_ini << endl;
	Nethead << "#" << "err" << " -> " << l_err << endl;
	Nethead << "#" << "end" << " -> " << l_end << endl;
	Nethead << "#" << "nil" << " -> " << l_nil << endl;
	Nethead << endl;		
	ofstream NETf(base + "net_mocinc.tts"); NETf << Nethead.str() << n_move_inconsistent << endl;
	ofstream NETfnm(base + "net_ignmon.tts"); NETfnm << Nethead.str() << n_ignore_non_mon << endl;

	n.reduce_log << n << endl;
}
