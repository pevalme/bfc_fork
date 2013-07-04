#include "net.h"

class ticketabs{

	enum PCt{
		PC1 = 0,
		PC2 = 1,
		PC3 = 2,
		PCE = 3
	};

	const local_t	l_ini,l_err,l_end,l_nil,L;
	const shared_t	s_ini,s_err,S;

	unsigned N, M; //N: #threads considered in the abstraction, M: data-range of concrete variables is [0,M]

	bool consistent(unsigned,bool) const;
	
	bool PRED(unsigned,unsigned,bool) const;

public:
	
	struct rel_t
	{
		typedef unsigned long long T;

		//static const unsigned D = 5; //output formatting
		static const unsigned D = 4; //output formatting
		static const unsigned L = 2*2*2; //number of local states per program location
		static const unsigned truefalse = 3;

		union {
			T val; //4 byte in VS12 (32bit)
			struct {
				//encoding
				//4x16bit
				//pc in 0,1,2 // 3=*
				//0=false,1=true,3=* //2=unused
				//3=unused

				//next-state
				unsigned b31p : 4;			
				unsigned b21p : 4;
				unsigned b11p : 4;
				unsigned pc1p : 4; //passive (1st bit)
				unsigned b3p  : 4;
				unsigned b2p  : 4;
				unsigned b1p  : 4;
				unsigned pcp  : 4; //active (1st bit)
				//current-state
				unsigned b31  : 4;
				unsigned b21  : 4;
				unsigned b11  : 4;
				unsigned pc1  : 4; //passive (1st bit)
				unsigned b3   : 4;
				unsigned b2   : 4;
				unsigned b1   : 4; 
				unsigned pc   : 4; //active (1st bit)
			};
		};

		rel_t(T = 0);
		rel_t(unsigned, unsigned, unsigned, unsigned);
		rel_t(unsigned, bool);

		T C ();
		T N ();
		T CE(); //"quantify out" passive current-state variable (1: keep only active-current, 2: set passive-current to "F")
		T NE();

		static string CH(bool);
		string C_str(bool);
		static string NH(bool);
		string N_str(bool);
		
		unsigned C_numa() const;
		unsigned C_nump() const;
		unsigned N_numa() const;
		unsigned N_nump() const;

		static void test();

		bool operator < (const rel_t& r) const;
	
	};

	//ticketabs(unsigned N = 4, unsigned M = 3);
	ticketabs(unsigned N = 2, unsigned M = 3);

	void initial_net(Net&);
	void update_transitions(Net&);
	void test(Net&);

};