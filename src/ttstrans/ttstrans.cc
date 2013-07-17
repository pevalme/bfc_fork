#include <iostream>
#include "boost/program_options.hpp"
using namespace boost::program_options;
#include "boost/lexical_cast.hpp"
#include <vector>
#include <string>

#include "user_assert.h"
#include "types.h"

using namespace std;

#include "net.h"
Net net;

void print(const Net::adj_t& adj, trans_type t_ty, bool hdr, Net::net_format_t n_ty, ostream& out, Net& net)
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
				out << "l" << t.local << ">=1, " << "s" << t.shared << ">=1 -> " << endl;
				if(t.shared != succ.shared) out << "\ts" << t.shared << "'=s" << t.shared << "-1," << endl << "\ts" << succ.shared << "'=s" << succ.shared << "+1";
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
			(OPT_STR_HELP, bool_switch(&(bool&)h), "produce help message")
			(OPT_STR_INPUT_FILE, value<string>(&i), "input file")
			(OPT_STR_INIT, value<string>(&init_fn)->default_value(OPT_STR_INIT_VAL_PARA), "initial state (e.g. 1|0 or 1|0,1/2)")
			(OPT_STR_TARGET, value<string>(&target_fn), "target state file (e.g. 1|0,1,1)")
			(OPT_STR_OUTPUT_FILE, value<string>(&o)->default_value(OPT_STR_OUTPUT_FILE_DEFVAL), "output file (\"stdout\" for console)")
			(OPT_STR_FORMAT, value<string>(&f), (string("output format: ")
			+ '"' + OPT_STR_FORMAT_CLASSIFY + '"' + ", "
			+ '"' + OPT_STR_FORMAT_MIST + '"' + ", "
			+ '"' + OPT_STR_FORMAT_TIKZ + '"' + ", "
			+ '"' + OPT_STR_FORMAT_LOLA + '"' + ", "
			+ '"' + OPT_STR_FORMAT_TINA + '"' + ", "
			+ '"' + OPT_STR_FORMAT_TTS + '"').c_str())
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
		print(net.adjacency_list, thread_transition, true, format, cout, net);
	}

	switch(format)
	{
	case Net::TTS: 
		break;
	
	case Net::MIST: 
		{
			cout << endl << "init" << endl;
			foreach(local_t l, net.init.bounded_locals)
				cout << "l" << l << "=1, ";
			foreach(local_t l, net.init.unbounded_locals)
				cout << "l" << l << ">=1, ";
			cout << "s" << net.init.shared << "=1" << endl;
			cout << endl << "target" << endl;
			cout << "s" << net.target.shared << ">=1";
			foreach(local_t l_, set<local_t>(net.target.bounded_locals.begin(),net.target.bounded_locals.end()))
				cout << "," << "l" << l_ << ">=" << net.target.bounded_locals.count(l_);
			cout << endl;
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
