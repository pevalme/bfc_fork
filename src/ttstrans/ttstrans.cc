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
#include <iostream>
#include "boost/program_options.hpp"
using namespace boost::program_options;
#include "boost/lexical_cast.hpp"
#include <vector>
#include <string>

#include "user_assert.h"
#include "types.h"
#include <tgmath.h>
#include <bitset>

using namespace std;

#include "net.h"
Net net;

void print(const Net::adj_t& adj, trans_type t_ty, bool hdr, Net::net_format_t n_ty, ostream& out, Net& net, bool mist0, bool mistS, bool mistL)
{
	unsigned ctr = 0;
	string sep;
	if(hdr)//print header
	{
		switch(n_ty)
		{
		case Net::TTS:
			out << net.S << " " << net.L << endl;
			break;
		case Net::MIST:
			cout << "vars" << endl;
			if (mistS)
				cout << "s0 s1 ";
			else if (mistL)
				for(shared_t s_=0; s_ < 2*(floor(log2(net.S)) + 1); s_++) cout << "s" << s_ << " ";
			else
				for(shared_t s_=0; s_ < net.S; s_++) cout << "s" << s_ << " ";

			for(local_t  l_=0; l_ < net.L; l_++) cout << "l" << l_ << " ";
			cout << endl << endl << "rules" << endl;
			break;
		case Net::TIKZ:
			out << "\\draw[step=1,gray,very thin] (0,0) grid (" << net.L << "," << net.S << ");" << endl;
			out << "\\foreach \\l in {0,2,...," << net.L-1 << "} \\draw (\\l + 0.5," << net.S << ") node[anchor=south] {$\\scriptscriptstyle \\l$};" << endl;
			out << "\\foreach \\s in {0,2,...," << net.S-1 << "} \\draw (0," << net.S-1 << "- \\s + 0.5) node[anchor=east] {$\\scriptscriptstyle \\s$};" << endl;
			out << "\\draw[-|] (0," << net.S << ") -- (" << net.L << "," << net.S << ") node[right] {$\\scriptscriptstyle l$};" << endl;
			out << "\\draw[-|] (0," << net.S << ") -- (0,0) node[below] {$\\scriptscriptstyle s$};" << endl;
			for(unsigned int s = 0; s<net.S; ++s)
				for(unsigned int l = 1; l<net.L; ++l)
					out << "\\draw (" << l << ".5," << net.S-s-1 << ".5) node{$\\phantom{\\scriptscriptstyle 0}$};" << endl;
			break;
		case Net::LOLA:
			//PLACE x0,x1,x2,x3,x4,x5,x6,x7,x8,x9,x10,x11,x12,x13,x14,x15,x16,x17,x18,x19,x20,x21,x22,x23,x24,x25,x26,x27;
			out << "PLACE ", sep = "";
			for(shared_t s_=0; s_ < net.S; s_++) out << sep << "s" << s_, sep = ',';
			for(local_t  l_=0; l_ < net.L; l_++) out << sep << "l" << l_, sep = ',';
			out << ";" << endl;

			//MARKING x0:1,x27:1;
			//Lola does not support initial omega markings, self-loop transitions instead
			out << endl << "MARKING ";
			foreach(local_t l, net.init.bounded_locals)
				out << "l" << l << ":1,";
			out << "s" << net.init.shared << ":1" << ";" << endl;
			cout << endl;

			//TRANSITION ini
			//CONSUME ;
			//PRODUCE x0:1;
			foreach(local_t l, net.init.unbounded_locals)
			{
				out << "TRANSITION inilocal" << l << endl;
				out << "CONSUME ;" << endl;
				out << "PRODUCE l" << l << ":1;" << endl;
				out << endl;
			}
			break;
		case Net::TINA:
			assert(0);
			break;
		}
	}

	for(auto& pair : adj)
	{
		const Thread_State& t = pair.first;
		auto& successors = pair.second;
		for(auto& succt : successors) //for (set<std::pair<Thread_State, transfers_t > >::const_iterator succt = successors.begin(), end = successors.end(); succt != end; ++succt)
		{

			const Thread_State& succ = succt.first;
			const transfers_t& p = succt.second;

			if(!p.empty()) throw runtime_error("translation of broadcast transitions not supported");

			switch(n_ty){
			case Net::TTS:
				switch(t_ty){
				case thread_transition: out << t << " -> " << (succ) << endl; break;
				case transfer_transition: out << t << " ~> " << (succ) << endl; break;
				case spawn_transition: out << t << " +> " << (succ) << endl; break;
				default: assert(0);}
				break;

			case Net::MIST:
				if (mistS) {
					out  << "l" << t.local << ">=1, " << "s0>=" << t.shared << ", s1>=" << net.S-t.shared -1<< " -> " << endl;
					if(t.shared != succ.shared) {
						if (succ.shared >= t.shared) {
							out << "\ts0'=s0+" << succ.shared - t.shared << ",\n\ts1'=s1-" << succ.shared - t.shared;
						} else {
							out << "\ts0'=s0-" << t.shared - succ.shared  << ",\n\ts1'=s1+" << t.shared - succ.shared;
						}
					}
				} else if (mistL){
					bitset<32> new_state = succ.shared, old_state = t.shared;
					int i;

					out  << "l" << t.local << ">=1";

					for(i=0; i < floor(log2(net.S)) + 1; i++){
						if(old_state[i] == 1) {
							out << ", s" << 2*i << ">=1";
							out << ", s" << 2*i+1 << ">=0";
						} else {
							out << ", s" << 2*i << ">=0";
							out << ", s" << 2*i+1 << ">=1";
						}
					}
					out << "->" << endl;
					//out << ", s" << floor(log2(net.S))+1 << ">=" << count1s << ", s" << floor(log2(net.S)) +2 << ">=" << floor(log2(net.S)) +1 - count1s << " -> " << endl;

					for(i=0; i <= floor(log2(net.S)) + 1; i++){
						if(new_state[i] != old_state[i]){ // If the bit has change
							if(new_state[i] == 1) {
								out << "\ts" << 2*i << "'=s" << 2*i << "+1," << endl;
								out << "\ts" << 2*i+1 << "'=s" << 2*i+1 << "-1";
							} else {
								out << "\ts" << 2*i << "'=s" << 2*i << "-1,"<< endl;
								out << "\ts" << 2*i+1 << "'=s" << 2*i+1 << "+1";
							}
							break;
						}
					}

					i++;

					for(; i <= floor(log2(net.S)) + 1; i++){
						if(new_state[i] != old_state[i]){ // If the bit has change
							if(new_state[i] == 1) {
								out << ",\n\ts" << 2*i << "'=s" << 2*i << "+1";
								out << ",\n\ts" << 2*i+1 << "'=s" << 2*i+1 << "-1";
							} else {
								out << ",\n\ts" << 2*i << "'=s" << 2*i << "-1";
								out << ",\n\ts" << 2*i+1 << "'=s" << 2*i+1 << "+1";
							}
						}
					}

					/*if (new1s > 0) {
						out << ",\n\ts" << floor(log2(net.S))+1 << "'=s" << floor(log2(net.S))+1 << "+" << new1s << ",\n";
						out << "\ts" << floor(log2(net.S))+2 << "'=s" << floor(log2(net.S))+2 << "-" << new1s;
					} else if (new1s < 0){
						out << ",\n\ts" << floor(log2(net.S))+1 << "'=s" << floor(log2(net.S))+1 << "-" << -new1s << ",\n";
						out << "\ts" << floor(log2(net.S))+2 << "'=s" << floor(log2(net.S))+2 << "+" << -new1s;
					}*/


				} else {
					out << "l" << t.local << ">=1, " << "s" << t.shared << ">=1 -> "

					<< endl;
					if(t.shared != succ.shared) out << "\ts" << t.shared << "'=s" << t.shared << "-1," << endl << "\ts" << succ.shared << "'=s" << succ.shared << "+1";
				}

				if(t.local != succ.local)
				{
					if(t.shared != succ.shared) out << "," << endl;
					switch(t_ty){
					case transfer_transition: out << "\tl" << t.local << "'=0," << endl << "\tl" << succ.local << "'=l" << succ.local << "+l" << t.local << ";"; break;
					case thread_transition: out << "\tl" << t.local << "'=l" << t.local << "-1," << endl << "\tl" << succ.local << "'=l" << succ.local << "+1;"; break;
					case spawn_transition: out << "\tl" << succ.local << "'=l" << succ.local << "+1;"; break;
					default: assert(0);}
				}
				else out << ";";
				out << endl << endl;
				break;

			case Net::TIKZ:
				out << "\\draw[";
				switch(t_ty){
					case transfer_transition: out << "transfer_trans"; break;
					case thread_transition: out << "thread_trans"; break;
					case spawn_transition: out << "spawn_trans"; break;
					default: assert(0);}
				out << "] (" << t.local << "+0.5," << net.S-t.shared-1 << "+0.5) to (" << succ.local << "+0.5," << net.S-succ.shared-1 << "+0.5);" << endl;
				break;

			case Net::LOLA:
				//s l -> s' l'
				//TRANSITION t8
				//CONSUME {s}:1,{l}:1;
				//PRODUCE {s'}:1,{l'}:1;

				//s l +> s' l'
				//TRANSITION t8
				//CONSUME {s}:1,{l}:1;
				//PRODUCE {s'}:1,{l'}:1,{l}:1;


				//TRANSITION t8
				out << "TRANSITION t";
				switch(t_ty){
				case thread_transition: out << "TR" << ctr++ << endl; break;
				case spawn_transition: out << "SP" << ctr++ << endl; break;
				default: assert(0);}

				//CONSUME x4:1,x26:1;
				out << "CONSUME " << "s" << t.shared<< ":1," << "l" << t.local << ":1;" << endl;

				//PRODUCE x0:1,x27:1;
				out << "PRODUCE ";
				switch(t_ty){
				case thread_transition: out << "s" << succ.shared << ":1," << "l" << succ.local << ":1;" << endl; break;
				case spawn_transition: out << "s" << succ.shared << ":1," << "l" << succ.local << ":1," << "l" << t.local << ":1;" << endl; break;
				default: assert(0);}
				break;

			case Net::TINA:
				assert(0);
				break;
			}
		}
	}
}

#include "options_str.h"
#include <fstream>
void test_g();

int main(int argc, char* argv[])
{

	string i,o;
	bool mist0=false, mistS=false, mistL=false;
	ofstream fout;

	Net::net_format_t format;

	//parse and check arguments
	try
	{
		bool single_initial(0);
		unsigned h(0);
		string f,target_fn,init_fn;

		options_description desc;
		desc.add_options()
			(OPT_STR_HELP, bool_switch(&(bool&)h), OPT_STR_HELP_HELP)
			(OPT_STR_INPUT_FILE, value<string>(&i), OPT_STR_INPUT_FILE_HELP)
			(OPT_STR_INIT, value<string>(&init_fn)->default_value(OPT_STR_INIT_VAL_PARA), OPT_STR_INIT_HELP)
			(OPT_STR_TARGET, value<string>(&target_fn), OPT_STR_TARGET_HELP)
			(OPT_STR_OUTPUT_FILE, value<string>(&o)->default_value(OPT_STR_OUTPUT_FILE_DEFVAL), "output file (\"stdout\" for console)")
			(OPT_STR_FORMAT, value<string>(&f), (string("output format: ")
			+ '"' + OPT_STR_FORMAT_CLASSIFY + '"' + ", "
			+ '"' + OPT_STR_FORMAT_MIST + '"' + ", "
			+ '"' + OPT_STR_FORMAT_TIKZ + '"' + ", "
			+ '"' + OPT_STR_FORMAT_LOLA + '"' + ", "
			+ '"' + OPT_STR_FORMAT_TINA + '"' + ", "
			+ '"' + OPT_STR_FORMAT_TTS + '"').c_str())
			(OPT_STR_MIST_OP_0, bool_switch(&(bool&)mist0), OPT_STR_MIST_OP_0_HELP)
			(OPT_STR_MIST_LOG_MIN, bool_switch(&(bool&)mistL), OPT_STR_MIST_LOG_MIN_HELP)
			(OPT_STR_MIST_MINIMIZE, bool_switch(&(bool&)mistS), OPT_STR_MIST_MINIMIZE_HELP)
			;
		variables_map vm;
		store(parse_command_line(argc, argv, desc), vm), notify(vm);

		//print help
		if(h){ std::cout << desc; return EXIT_FAILURE; }

		//check
		if     (f == OPT_STR_FORMAT_CLASSIFY) format = Net::CLASSIFY;
		else if(f == OPT_STR_FORMAT_MIST) format = Net::MIST;
		else if(f == OPT_STR_FORMAT_TIKZ) format = Net::TIKZ;
		else if(f == OPT_STR_FORMAT_TTS) format = Net::TTS;
		else if(f == OPT_STR_FORMAT_LOLA) format = Net::LOLA;
		else if(f == OPT_STR_FORMAT_TINA) format = Net::TINA;
		else throw runtime_error("no/invalid format");

		//read input net
		if(i == string()) throw logic_error("no input file");
		Net(i,target_fn,init_fn,false).swap(net);
		BState::S = net.S, BState::L = net.L;

		if(!net.target.consistent())
			throw std::runtime_error("invalid target state");
		if(!net.init.consistent())
			throw std::runtime_error("invalid initial state");

		//check output file
		if(o != "stdout")
		{
			fout.open(o.c_str());
			if(!fout.good()) throw std::runtime_error((string("cannot write to file ") + o).c_str());
		}
	}
	catch(std::exception& e) { cout << "INPUT ERROR: " << e.what() << "\n"; cout << "type " << argv[0] << " -h for instructions" << endl; return EXIT_FAILURE; }

	//redirect output stream
	std::streambuf* cout_sbuf;
	if(o != "stdout")
		cout_sbuf = std::cout.rdbuf(), // save original sbuf
		std::cout.rdbuf(fout.rdbuf()); // redirect 'cout' to a 'fout'

	//write output
	if(format == Net::CLASSIFY)
	{
#if 0
		Net::net_stats_t sts = net.get_net_stats(true);
		cout << "---------------------------------" << endl;
		cout << "Statistics for " << net.filename << endl;
		cout << "---------------------------------" << endl;
		cout << "local states                    : " << sts.L << endl;
		cout << "shared states                   : " << sts.S << endl;
		cout << "transitions                     : " << sts.T << endl;
		cout << "thread transitions (total)      : " << sts.trans_type_counters[thread_transition] << endl;
		cout << "thread transitions (%)          : " << (unsigned)((float)(100*sts.trans_type_counters[thread_transition])/sts.T) << endl;
		cout << "broadcast transitions (total)   : " << sts.trans_type_counters[transfer_transition] << endl;
		cout << "broadcast transitions (%)       : " << (unsigned)((float)(100*sts.trans_type_counters[transfer_transition])/sts.T) << endl;
		cout << "horizontal transitions (total)  : " << sts.trans_dir_counters[hor] << endl;
		cout << "horizontal transitions (%)      : " << (unsigned)((float)(100*sts.trans_dir_counters[hor])/sts.T) << endl;
		cout << "non-horizontal trans. (total)   : " << sts.trans_dir_counters[nonhor] << endl;
		cout << "non-horizontal trans. (%)       : " << (unsigned)((float)(100*sts.trans_dir_counters[nonhor])/sts.T) << endl;
		cout << "disconnected thread states      : " << sts.discond << endl;
		cout << "connected thread states (%)     : " << (unsigned)((float)(100*((sts.S * sts.L) - sts.discond)/(sts.S * sts.L))) << endl;
#ifdef DETAILED_TTS_STATS
		cout << "strongly connected components   : " << sts.SCC_num << endl;
		cout << "SCC (no disc. thread states)    : " << sts.SCC_num - sts.discond << endl;
#endif
		cout << "maximum indegree                : " << sts.max_indegree << endl;
		cout << "maximum outdegree               : " << sts.max_outdegree << endl;
		cout << "maximum degree                  : " << sts.max_degree << endl;
		cout << "---------------------------------" << endl;
#endif
	}
	else
	{
		print(net.adjacency_list, thread_transition, true, format, cout, net, mist0, mistS, mistL);
	}

	switch(format)
	{
	case Net::TTS:
		break;

	case Net::MIST:
		{
			cout << endl << "init" << endl;
			foreach(local_t l, set<local_t>(net.init.bounded_locals.begin(),net.init.bounded_locals.end()))
				cout << "l" << l << "=" << net.init.bounded_locals.count(l) << ", ";

			foreach(local_t l, net.init.unbounded_locals)
				cout << "l" << l << ">=1, ";
			if (mistS) {
				cout << "s0=" << net.init.shared << ", s1=" << net.S-net.init.shared-1 << endl;
			} else if (mistL){
				bitset<32> new_state = net.init.shared;

				cout << "s0=" << new_state[0];
				if (new_state[0] == 1) cout << ", s1=" << "0";
				else cout << ", s1=" << "1";

				for(int i=1; i < floor(log2(net.S)) + 1; i++) {
					cout << ", s" << 2*i << "=" << new_state[i];
					if (new_state[i] == 1) cout << ", s" << 2*i+1 << "=0";
					else cout << ", s" << 2*i+1 << "=1";
				}

				//cout << ", s" << floor(log2(net.S)) + 1 << "=" << count1s  << ", s" << floor(log2(net.S)) + 2 << "=" << floor(log2(net.S)) + 1 - count1s << endl;
			} else {
				if (mist0){
					for(shared_t s_=0; s_ < net.S; s_++) {
						if (s_ != net.init.shared)
							cout << "s" << s_ << "=0, ";
					}
				}
				cout << "s" << net.init.shared << "=1" << endl;
			}

			cout << endl << "target" << endl;
			if (mistS) {
				cout << "s0>=" << net.target.shared << ", s1>=" << net.S-net.target.shared-1;
			} else if (mistL){
				bitset<32> new_state = net.target.shared;

				cout << "s0" << ">=" << new_state[0];
				if (new_state[0] == 1) cout << ", s1=" << "0";
				else cout << ", s1=" << "1";

				for(int i=1; i < floor(log2(net.S)) + 1; i++) {
					cout << ",s" << 2*i << ">=" << new_state[i];
					if (new_state[i] == 1) cout << ",s" << 2*i << ">=0";
					else cout << ",s" << 2*i+1 << ">=1";
				}

				//cout << ",s" << floor(log2(net.S)) + 1 << ">=" << count1s << ",s" << floor(log2(net.S)) + 2 << ">=" << floor(log2(net.S)) + 1 - count1s;
			} else {
				if(mist0){
					for(shared_t s_=0; s_ < net.S; s_++) {
						if (s_ != net.target.shared)
							cout << "s" << s_ << ">=0, ";
					}
				}
				cout << "s" << net.target.shared << ">=1";

			}

			foreach(local_t l_, set<local_t>(net.target.bounded_locals.begin(),net.target.bounded_locals.end()))
				cout << "," << "l" << l_ << ">=" << net.target.bounded_locals.count(l_);
			cout << endl;

			cout << endl << "invariants" << endl;

			if(mistL){
				int i;
				cout << "s0=1";

				for(i=1; i < 2*(floor(log2(net.S)) + 1); i++) {
					cout << ", s" << i << "=1";
				}

			} else if(mistS){
				cout << "s0=1, s1=1" << endl;
			} else{
				for(shared_t s_=0; s_ < net.S - 1; s_++) cout << "s" << s_ << "=1, ";
				cout << "s" << net.S-1 << "=1" << endl;
			}

			break;
		}

	case Net::TIKZ:
		break;

	case Net::LOLA:
		break;

	case Net::TINA:
		assert(0);
		break;
	}

	//restore output stream
	if(o != OPT_STR_OUTPUT_FILE_STDOUT)
		std::cout.rdbuf(cout_sbuf), // restore the original stream buffer
		cout << "output written to " << o << endl;

}
