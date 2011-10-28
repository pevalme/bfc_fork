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

void print(const Net::adj_t& adj, trans_type t_ty, bool hdr, Net::net_format_t n_ty, ostream& out)
{
	if(hdr)//print header
	{
		switch(n_ty)
		{
		case Net::TTS: 
			out << BState::S << " " << BState::L << endl; break;
		case Net::MIST: 
			cout << "vars" << endl;
			for(shared_t s_=0; s_ < BState::S; s_++) cout << "s" << s_ << " ";
			for(local_t  l_=0; l_ < BState::L; l_++) cout << "l" << l_ << " ";
			cout << endl << endl << "rules" << endl; break;
		case Net::TIKZ: 
			out << "\\draw[step=1,gray,very thin] (0,0) grid (" << BState::L << "," << BState::S << ");" << endl;
			out << "\\foreach \\l in {0,2,...," << BState::L-1 << "} \\draw (\\l + 0.5," << BState::S << ") node[anchor=south] {$\\scriptscriptstyle \\l$};" << endl;
			out << "\\foreach \\s in {0,2,...," << BState::S-1 << "} \\draw (0," << BState::S-1 << "- \\s + 0.5) node[anchor=east] {$\\scriptscriptstyle \\s$};" << endl;
			out << "\\draw[-|] (0," << BState::S << ") -- (" << BState::L << "," << BState::S << ") node[right] {$\\scriptscriptstyle l$};" << endl;
			out << "\\draw[-|] (0," << BState::S << ") -- (0,0) node[below] {$\\scriptscriptstyle s$};" << endl;
			for(unsigned int s = 0; s<BState::S; ++s)
				for(unsigned int l = 1; l<BState::L; ++l)
					out << "\\draw (" << l << ".5," << BState::S-s-1 << ".5) node{$\\phantom{\\scriptscriptstyle 0}$};" << endl;
			break;
		}
	}
	
	for (Net::adj_t::const_iterator pair = adj.begin(), end = adj.end(); pair != end; ++pair) //print transisitons
	{
		const Thread_State&      t          = pair->first;
		const set<Thread_State>& successors = pair->second;
		for (set<Thread_State>::const_iterator succ = successors.begin(), end = successors.end(); succ != end; ++succ)
		{
			switch(n_ty){
			case Net::TTS:
				switch(t_ty){
				case thread_transition: out << t << " -> " << (*succ) << endl; break;
				case transfer_transition: out << t << " ~> " << (*succ) << endl; break;}
				break;
			
			case Net::MIST:
				out << "l" << t.local << ">=1, " << "s" << t.shared << ">=1 -> " << endl;
				if(t.shared != succ->shared) out << "\ts" << t.shared << "'=s" << t.shared << "-1," << endl << "\ts" << succ->shared << "'=s" << succ->shared << "+1";
				if(t.local != succ->local)
				{
					if(t.shared != succ->shared) out << "," << endl;
					switch(t_ty){
					case transfer_transition: out << "\tl" << t.local << "'=0," << endl << "\tl" << succ->local << "'=l" << succ->local << "+l" << t.local << ";"; break;
					case thread_transition: out << "\tl" << t.local << "'=l" << t.local << "-1," << endl << "\tl" << succ->local << "'=l" << succ->local << "+1;"; break;}
				}
				else out << ";";
				out << endl << endl;
				break;

			case Net::TIKZ: 
				out << "\\draw[" << (t_ty == transfer_transition?"transfer_trans":"thread_trans") << "] (" << t.local << "+0.5," << BState::S-t.shared-1 << "+0.5) to (" << succ->local << "+0.5," << BState::S-succ->shared-1 << "+0.5);" << endl; 
				break;
			}
		}
	}
}

#include "options_str.h"

void test_g();

int main(int argc, char* argv[]) 
{

	unsigned s,l;
	string i,o;
	bool check_target(0);
	ofstream fout;

	Net net;
	BState target;
	Net::net_format_t format;

	//parse and check arguments
	try
	{
		unsigned h(0);
		string f,t;

		options_description desc;
		desc.add_options()			
			(OPT_STR_HELP, bool_switch(&(bool&)h), "produce help message")
			(OPT_STR_INI_SHARED, value<unsigned>(&s)->default_value(OPT_STR_INI_SHARED_DEFVAL), "initial shared state")
			(OPT_STR_INI_LOCAL, value<unsigned>(&l)->default_value(OPT_STR_INI_LOCAL_DEFVAL), "initial local state")
			(OPT_STR_INPUT_FILE, value<string>(&i), "input file")
			(OPT_STR_TARGET, value<string>(&t), "target input file")
			(OPT_STR_OUTPUT_FILE, value<string>(&o)->default_value(OPT_STR_OUTPUT_FILE_DEFVAL), "output file (\"stdout\" for console)")
			(OPT_STR_FORMAT, value<string>(&f), (string("output format: ")
			+ '"' + OPT_STR_FORMAT_CLASSIFY + '"' + ", "
			+ '"' + OPT_STR_FORMAT_MIST + '"' + ", "
			+ '"' + OPT_STR_FORMAT_TIKZ + '"' + ", "
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
		else throw runtime_error("no/invalid format");

		//read input net
		if(i == string()) throw logic_error("no input file");
		net.read_net_from_file(i);
		
		//get initial state
		net.init.shared = s, net.init.unbounded_locals.insert(l);
		if(!net.init.consistent()) throw runtime_error("invalid initial state");
		
		//get target
		if(t != string())
		{
			ifstream target_in(t.c_str());
			if(!target_in.good()) throw runtime_error("cannot read from target input file");
			string target_str; target_in >> target_str, target = BState(target_str);
			if(!target.consistent()) throw runtime_error("invalid target");
			check_target = 1;
		}

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
		Net::net_stats_t sts = net.get_net_stats();
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
		cout << "strongly connected components   : " << sts.SCC_num << endl;
		cout << "SCC (no disc. thread states)    : " << sts.SCC_num - sts.discond << endl;
		cout << "maximum indegree                : " << sts.max_indegree << endl;
		cout << "maximum outdegree               : " << sts.max_outdegree << endl;
		cout << "maximum degree                  : " << sts.max_degree << endl;
		cout << "---------------------------------" << endl;
	}
	else
	{
		print(net.adjacency_list, thread_transition, true, format, cout);
		print(net.adjacency_list, transfer_transition, false, format, cout);
	}

	switch(format)
	{
	case Net::TTS: 
		break;
	
	case Net::MIST: 
		{
			cout << endl << "init" << endl << "l" << l << ">=1, s" << s << "=1" << endl;
			cout << endl << "target" << endl;
			if(check_target)
			{
				cout << "s" << target.shared << ">=1";
				foreach(local_t l_, set<local_t>(target.bounded_locals.begin(),target.bounded_locals.end()))
					cout << "," << "l" << l_ << ">=" << target.bounded_locals.count(l_);
				cout << endl;
			}
			break;
		}

	case Net::TIKZ:
		break;

	}

	//restore output stream
	if(o != OPT_STR_OUTPUT_FILE_STDOUT)
		std::cout.rdbuf(cout_sbuf), // restore the original stream buffer 
		cout << "output written to " << o << endl;

}
