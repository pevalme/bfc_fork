/******************************************************************************
  Synopsis		[Bfc - Greedy Analysis of Multi-Threaded Programs with 
				Non-Blocking Communication.]

  Author		[Alexander Kaiser]

(C) 2011 - 2014 Alexander Kaiser, University of Oxford, United Kingdom

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
//#define PSEUDO_RNG
#define TTSGEN_VERSION "0.1"

#include "boost/program_options.hpp"
using namespace boost::program_options;
#include "boost/random.hpp"
#include "boost/lexical_cast.hpp"
#include "tstate.h"
#include <string>
#include <fstream>
#include <set>
#include "boost/date_time.hpp"
#include "trans.h"

using namespace std;

#include <boost/random/random_device.hpp>
#include <boost/random/uniform_int_distribution.hpp>

unsigned count_dir_successors(set<Thread_State>& sucs, shared_t s, trans_dir_t dir)
{
	unsigned count = 0;

	for(auto b = sucs.begin(), e = sucs.end(); b != e; ++b)
		switch(dir){
		case hor: if(b->shared == s) ++count; break;
		case nonhor: if(b->shared != s) ++count; break;}
		
	return count;
}

#include "options_str.h"

int main(int argc, char* argv[]) 
{

	unsigned s,l,te,be,he,tc,is,il,h(0),v(0),T,e_max,unstructured(0);
	const unsigned diff(10);
	string ofile;
	ofstream fout;

	//parse and check arguments
	try
	{
		options_description misc("Generic",LINE_LENGTH,MIN_DESC_LENGTH);
		misc.add_options()
			(OPT_STR_HELP, bool_switch(&(bool&)h), "produce help message\n")
			(OPT_STR_VERSION, bool_switch(&(bool&)v), "print version info and exit\n")
			(OPT_STR_OUTPUT_FILE, value<string>(&ofile)->default_value(OPT_STR_OUTPUT_FILE_DEFVAL), ((string)"output file (\"" + OPT_STR_OUTPUT_FILE_STDOUT +"\" for console)").c_str())
			;

		options_description dimension("Dimension",LINE_LENGTH,MIN_DESC_LENGTH);
		dimension.add_options()
			(OPT_STR_NUM_SHARED, value<unsigned>(&s)->default_value(OPT_STR_NUM_SHARED_DEFVAL), "number of shared states\n")
			(OPT_STR_NUM_LOCALS, value<unsigned>(&l)->default_value(OPT_STR_NUM_LOCALS_DEFVAL), "number of local states")
			;

		options_description transitions("Transitions",LINE_LENGTH,MIN_DESC_LENGTH);
		transitions.add_options()
			(OPT_STR_NUM_TRANS, value<unsigned>(&te)->default_value(OPT_STR_NUM_TRANS_DEFVAL), "transitions (no self loops; \"0\" for maximum value)\n")
			(OPT_STR_TRANS_SCALE, value<unsigned>(&tc)->default_value(OPT_STR_TRANS_SCALE_DEFVAL), "additional trasitions (multiplied by S*L)\n")
			(OPT_STR_BCST_RATIO, value<unsigned>(&be)->default_value(OPT_STR_BCST_RATIO_DEFVAL), "percentage of broadcast transitions\n")
			(OPT_STR_HOR_RATIO, value<unsigned>(&he)->default_value(OPT_STR_HOR_RATIO_DEFVAL), "percentage of horizontal transitions\n")
			(OPT_STR_UNSTRUCTURED_SW, bool_switch(&(bool&)unstructured), "generate more dead transitions")
			;

		options_description initial("Initial state (only for structured generation)",LINE_LENGTH,MIN_DESC_LENGTH);
		initial.add_options()			
			(OPT_STR_INI_SHARED, value<unsigned>(&is)->default_value(OPT_STR_INI_SHARED_DEFVAL), "initial shared state\n")
			(OPT_STR_INI_LOCAL, value<unsigned>(&il)->default_value(OPT_STR_INI_LOCAL_DEFVAL), "initial local state")
			;

		options_description cmdline_options;
		cmdline_options
			.add(misc)
			.add(dimension)
			.add(transitions)
			.add(initial)
			;

		variables_map vm;
		store(parse_command_line(argc, argv, cmdline_options), vm), notify(vm); 

		//print help/version
		if(h || v)
		{ 
			if(h) cout	<< "Usage: " << argv[0] << " [options]" << endl << cmdline_options; 
			if(v) cout 
				<< "                      TTS Generator\n"
				<< "                               v" << TTSGEN_VERSION << "\n"
#ifdef PSEUDO_RNG
				<< "         using pseudorandom numbers\n"
#else
				<< "        using \"true\" random numbers\n"
#endif
				<< "-----------------------------------\n"
				<< "     (c)2011 Alexander Kaiser      \n"
				<< " Oxford University, United Kingdom \n";

			return EXIT_SUCCESS;
		}

		//check dimension arguments
		if((s * l) != (unsigned long)(s * l)) throw std::runtime_error("dimension too large");
		if((te + (s*l*tc/10)) != (unsigned long)(te + (s*l*tc/10))) throw std::runtime_error("too many edges");
		
		//scale and check edge argument
		if(be > 100) throw std::runtime_error("invalid ratio");
		te = te+(s*l*tc/diff), T = s * l, e_max = T * (T - 1); // number of thread states and maximum transitions

		if(te > e_max || be > e_max || te+be == 0) throw std::runtime_error((string("maximum number of edges/scale factor in this configuration: ") + boost::lexical_cast<string>(e_max) + "/" + boost::lexical_cast<string>(e_max/(s*l/10))).c_str());
		if(is >= s || il >= l) throw std::runtime_error("invalid initial shared/local state");
		
		if(unstructured)
		{
			unsigned he_total = he*((float)te/100), he_max = s*l*(l-1);
			if(he_total > he_max) throw logic_error("too many horizontal transitions");
	
			unsigned vde_total = te - (he*((float)te/100)), vde_max = s*l*l*(s-1);
			if(vde_total > vde_max) throw logic_error("too many vertical/diagonal transitions");
		}
	}
	catch(std::exception& e) { cout << "INPUT ERROR: " << e.what() << "\n"; cout << "type " << argv[0] << " -h for instructions" << endl; return EXIT_FAILURE; }

	//create transition relation
	boost::uniform_int<> locals_range(0,l-1), shared_range(0,s-1), tans_range(0,te-1);
#ifdef PSEUDO_RNG
	boost::mt19937 rng(boost::posix_time::microsec_clock::local_time().time_of_day().total_microseconds()); //std::time(0) will only generate one tts per second
	boost::variate_generator<boost::mt19937&, boost::uniform_int<> > locals_die(rng, locals_range), shared_die(rng, shared_range), tans_die(rng, tans_range); //random generator
#else
    boost::random::random_device	rng;
	boost::variate_generator<boost::random::random_device&, boost::uniform_int<> > locals_die(rng, locals_range), shared_die(rng, shared_range), tans_die(rng, tans_range); //random generator
#endif


	//todo: not so easy, e.g. with structured+he=100, he_max is (l-1) !!!; Hence, he_max cannot be easily computed. Perhaps count number of tries during generation and quit after a fixed number of invalid where created. Or do unstrucutred
	//do static test for unstructured generation and otherwise dynamic checks; also move these checks futher up since this is not try-catch block
	try
	{
		vector<bool> b_dist(te,0), h_dist(te,0);
		set<unsigned> b_seen, h_seen;
		unsigned t;

		unsigned be_num = be*((float)te/100);
		for(unsigned i = 0; i < be_num; ++i)
		{
			do t = tans_die(); 
			while(!b_seen.insert(t).second);
			b_dist[t] = 1;
		}

		unsigned he_num = he*((float)te/100);
		for(unsigned i = 0; i < he_num; ++i)
		{
			do t = tans_die();
			while(!h_seen.insert(t).second);
			h_dist[t] = 1;
		}

		std::map<Thread_State, std::set<Thread_State> > trel, trel_b;
		Thread_State src;
		std::set<Thread_State>* sucs;
		bool horizontal;

		vector<shared_t> shared_succs; shared_succs.push_back(is);
		vector<local_t> local_succs; local_succs.push_back(il);

		for(t = 0; t < te; ++t)
		{
			horizontal = h_dist[t];

			set<Thread_State> tries;
			do
			{
				if(!unstructured && tries.size() == shared_succs.size()*local_succs.size())
					throw logic_error("too many transition due to unstructured mode");

				src.shared = unstructured?shared_die():shared_succs[shared_die()%shared_succs.size()], src.local = unstructured?locals_die():local_succs[locals_die()%local_succs.size()]; //guarantees that at least the source local and shared state are "connected" to the previous transitions
				tries.insert(src);

				sucs = b_dist[t]?&trel_b[src]:&trel[src];
			}
			while ((horizontal && count_dir_successors(*sucs,src.shared,hor) == l - 1) || (!horizontal && count_dir_successors(*sucs,src.shared,nonhor) == s*l-l));

			while (true)
			{
				Thread_State succ(horizontal?src.shared:shared_die(),locals_die());
				bool valid_successor = (horizontal && succ.local != src.local) || (!horizontal && succ.shared != src.shared);
				if (!sucs->count(succ) && valid_successor) 
				{ 
					sucs->insert(succ);

					if(!unstructured)
					{
						if(find(shared_succs.begin(),shared_succs.end(),succ.shared) == shared_succs.end())
							shared_succs.push_back(succ.shared);
						if(find(local_succs.begin(),local_succs.end(),succ.local) == local_succs.end())
							local_succs.push_back(succ.local);
					}

					break;
				}
			}
		}

		//save
		std::streambuf* cout_sbuf;
		if(ofile != "stdout")
		{
			fout.open(ofile.c_str()); //this should not be done before the tts was created, since this can fail "dynamically" when generating an structured net
			if(!fout.good()) throw std::runtime_error((string("cannot write to file ") + ofile).c_str());
			cout_sbuf = std::cout.rdbuf(), std::cout.rdbuf(fout.rdbuf()); // save original sbuf and redirect 'cout' to a 'fout'
		}

		cout << s << " " << l << std::endl;
		for (auto pair = trel.begin(), end = trel.end(); pair != end; ++pair)
			for (auto succ = pair->second.begin(), end = pair->second.end(); succ != end; ++succ)
				cout << pair->first << " -> " << *succ << std::endl; 
		for (auto pair = trel_b.begin(), end = trel_b.end(); pair != end; ++pair)
			for (auto succ = pair->second.begin(), end = pair->second.end(); succ != end; ++succ)
				cout << pair->first << " ~> " << *succ << std::endl; 

		if(ofile != "stdout")
			std::cout.rdbuf(cout_sbuf), // restore the original stream buffer 
			cout << "output written to " << ofile << endl,
			fout.close();
	}
	catch(std::exception& e)
	{ std::cout << "error during generation: " << e.what() << std::endl; return EXIT_FAILURE; }

	return EXIT_SUCCESS;

}
