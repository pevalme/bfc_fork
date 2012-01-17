//#define PUBLIC_RELEASE
#define IMMEDIATE_STOP_ON_INTERRUPT //deactive to allow stopping the oracle with CTRG-C

/*

known bugs: 
	- regression/hor_por_vs_03 failed sometimes (seg. fault)
	- sometimes hangs at "Forward trace to target covering state found..."

*/

/******************************************************************************
  Synopsis		[Bfc - Greedy Analysis of Multi-Threaded Programs with 
				Non-Blocking Communication.]

  Author		[Alexander Kaiser]

(C) 2011 Alexander Kaiser, University of Oxford, United Kingdom

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

#define EXIT_VERIFICATION_SUCCESSFUL 0
#define EXIT_VERIFICATION_FAILED 10

#define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#if !defined WIN32 && GCC_VERSION < 40601
#error need g++ >= 4.6.1
#endif

//#define EAGER_ALLOC
#define VERSION "1.0"

#include "types.h"
#include "user_assert.h"
#include "net.h"
#include "ostate.h"
#include "bstate.h"
#include "complement.h"

unsigned max_fw_width = 0;
unsigned fw_threshold = 0;
#include "bfc.interrupts.h"
#include "options_str.h"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>

using namespace std;
using namespace boost::program_options;
using namespace boost::posix_time;

Net net;

boost::posix_time::ptime 
	last_new_prj_found,
	fw_start_time,
	finish_time
	;

shared_cmb_deque_t 
	shared_cmb_deque
	;

volatile bool 
	shared_fw_done(0),
	shared_bw_done(0),
	shared_fw_finised_first(0),
	shared_bw_finised_first(0)
	;

bool 
	writetrace(0),
	readtrace(0),
	bw_safe(1), //false, if bw found a viol. conf
	fw_safe(1), //false, if fw found a viol. conf
	print_hash_info(0)
	;

enum weigth_t
{
	order_width,
	order_depth,
	order_random
} bw_weight, fw_weight;

bool fw_state_blocked = false;
int kfw = -1;
#include "fw.h"
bool defer = false;
#include "bw.h"



void print_bw_stats(unsigned sleep_msec, string o)
{
	if(sleep_msec == 0) return;

	//const unsigned dst = 7;
	const unsigned sdst = 5;
	const unsigned ldst = 7;
	//const string sep = "|";
	const string sep = ",";

	ostream logStream(std::cout.rdbuf());
	ofstream csvFile;
	if(o != OPT_STR_OUTPUT_FILE_STDOUT)
	{
		csvFile.open(o.c_str());
		if(!csvFile.good()) throw std::runtime_error((string("cannot write to file ") + o).c_str());
		logStream.rdbuf(csvFile.rdbuf()); // redirect output to our log file
	}

	logStream
		<< "BCDep" << sep
		<< "BCWid" << sep
		<< "BDept" << sep
		<< "BWidt" << sep
		<< "BItrs__" << sep//ldist 5
		<< "BPrun__" << sep//ldist 6
		<< "BCycF" << sep
		<< "BCycB" << sep
		<< "BNode__" << sep//ldist 9
		<< "osz____" << sep//ldist 9
		<< "dsz____" << sep//ldist 9
		<< "wdsz___" << sep//ldist 9
		<< "WNode__" << sep//ldist 10
		<< "GNode__" << sep//ldist 10
		<< "LMin___" << sep//ldist 10
		<< "GMin___" << sep//ldist 10
		<< "FDept" << sep
		<< "FWidt" << sep
		<< "FItrs__" << sep//ldist 13
		<< "FAccs__" << sep//ldist 14
		<< "FNode__" << sep//ldist 15
		<< "FWNod__" << sep//ldist 15
#ifndef WIN32
		<< "MemMB__" << sep//ldist 17
#endif
		<< endl
		;

	while(1) 
	{

		if(shared_bw_done || shared_fw_done)
			break;

		shared_cout_mutex.lock();

		logStream 
			//bw
			<< setw(sdst) << ctr_bw_curdepth << /*' ' << */sep
			<< setw(sdst) << ctr_bw_curwidth << /*' ' << */sep
			<< setw(sdst) << ctr_bw_maxdepth << /*' ' << */sep
			<< setw(sdst) << ctr_bw_maxwidth << /*' ' << */sep
			<< setw(ldst) << witeration << /*' ' << */sep
			<< setw(ldst) << piteration << /*' ' << */sep
			<< setw(sdst) << fpcycles << /*' ' << */sep
			<< setw(sdst) << bpcycles << /*' ' << */sep
			<< setw(ldst) << nsz + msz << /*' ' << */sep
			<< setw(ldst) << osz << /*' ' << */sep
			<< setw(ldst) << dsz << /*' ' << */sep
			<< setw(ldst) << wdsz << /*' ' << */sep
			<< setw(ldst) << wsz << /*' ' << */sep
			<< setw(ldst) << gsz << /*' ' << */sep
			<< setw(ldst) << ctr_locally_minimal << /*' ' << */sep
			<< setw(ldst) << ctr_globally_minimal << /*' ' << */sep
			//fw
			<< setw(sdst) << ctr_fw_maxdepth << /*' ' << */sep
			<< setw(sdst) << ctr_fw_maxwidth << /*' ' << */sep
			<< setw(ldst) << f_its << /*' ' << */sep
			<< setw(ldst) << accelerations << /*' ' << */sep
			<< setw(ldst) << fw_qsz << /*' ' << */sep
			<< setw(ldst) << fw_wsz << /*' ' << */sep
			//both
#ifndef WIN32
			<< setw(ldst) << memUsed()/1024/1024 << /*' ' << */sep
#endif
			<< endl
			, 
			logStream.flush()
			;

		shared_cout_mutex.unlock();

		boost::this_thread::sleep(boost::posix_time::milliseconds(sleep_msec));
	}
}

string time_diff_str(boost::posix_time::ptime a, boost::posix_time::ptime b)
{
	return boost::lexical_cast<std::string>(((float)(a - b).total_microseconds())/1000000);
}

vector<BState> read_trace(string trace_fn)
{
	vector<BState> work_sequence;

	ifstream w;
	w.open(trace_fn.c_str(),ifstream::in);

	if(!w.is_open()) throw logic_error((string("cannot read from input file ") + (net.filename + ".trace")).c_str());
	else cout << "trace input file " << (net.filename + ".trace") << " successfully opened" << endl;

	unsigned s_,l_,k_;
	w >> s_ >> l_ >> k_;
	if(!(s_ == BState::S && l_ == BState::L)) throw logic_error("trace has invalid dimensions");

	stringstream in;
	{
		string line;
		while (getline(w, line = "")) 
		{
			const size_t comment_start = line.find("#");
			in << ( comment_start == string::npos ? line : line.substr(0, comment_start) ) << endl; 
		}
	}
	w.close();

	string line;
	while (in){
		getline (in,line);
		BState s(line);
		if(s.type != BState::invalid)
			work_sequence.push_back(s);
	}
	w.close();

	return work_sequence;
}

int main(int argc, char* argv[]) 
{

	//unordered_set<bstate_t> sadsd;
	//cout << "sadsd.bucket_count " << sadsd.bucket_count() << endl;
	//unordered_set<bstate_t> sadsd2(0);
	//cout << "sadsd2.bucket_count " << sadsd2.bucket_count() << endl;

	//BState sad(213,1);
	//cout << sad.bl->blocked_by.bucket_count() << endl;
	//cout << sad.bl->blocks.bucket_count() << endl;
	//cout << sad.nb->pre.bucket_count() << endl;
	//cout << sad.nb->suc.bucket_count() << endl;

//	return 1;

	int return_value = EXIT_SUCCESS;

	srand((unsigned)microsec_clock::local_time().time_of_day().total_microseconds());

	enum {FW,BW,FWBW,CON,FW_CON} mode;
	unsigned k,ab(OPT_STR_ACCEL_BOUND_DEFVAL), mon_interval(OPT_STR_MON_INTERV_DEFVAL);
	unordered_priority_set<bstate_t>::order_t border;
	unordered_priority_set<ostate_t>::order_t forder;
	vector<BState> work_sequence;
	bool forward_projections(0), cross_check(0), print_cover(0);
	string o = OPT_STR_OUTPUT_FILE_DEFVAL;

	try {
		shared_t init_shared;
		local_t init_local;
		string filename, target_fn, border_str = OPT_STR_BW_ORDER_DEFVAL, forder_str = OPT_STR_FW_ORDER_DEFVAL, bweight_str = OPT_STR_WEIGHT_DEFVAL, fweight_str = OPT_STR_WEIGHT_DEFVAL, mode_str, graph_style = OPT_STR_BW_GRAPH_DEFVAL;
		bool h(0), v(0), print_sgraph(0), stats_info(0), print_preinf(0), print_bwinf(0), print_fwinf(0), single_initial(0), debug_info(1);
#ifndef WIN32
		unsigned to,mo;
#endif

		//Misc
		options_description misc;
		misc.add_options()
			(OPT_STR_HELP,			bool_switch(&h), "produce help message and exit")
			(OPT_STR_VERSION,		bool_switch(&v), "print version info and exit")
#ifndef PUBLIC_RELEASE
			(OPT_STR_CROSS_CHECK,	bool_switch(&cross_check), "compare results obtained with forward and backward")
#endif
			(OPT_STR_PRINT_KCOVER,	bool_switch(&print_cover), "print k-cover elements")
			(OPT_STR_STATS,			bool_switch(&stats_info), "print final statistics")
#ifndef PUBLIC_RELEASE
			(OPT_STR_MON_INTERV,	value<unsigned>(&mon_interval)->default_value(OPT_STR_MON_INTERV_DEFVAL), "update interval for on-the-fly statistics (ms, \"0\" for none)")
#endif
#ifndef WIN32
			(OPT_STR_TIMEOUT,		value<unsigned>(&to)->default_value(0), "CPU time in seconds (\"0\" for no limit)")
			(OPT_STR_MEMOUT,		value<unsigned>(&mo)->default_value(0), "virtual memory limit in megabyte (\"0\" for no limit)")
#endif
#ifndef PUBLIC_RELEASE
			(OPT_STR_BW_GRAPH,		value<string>(&graph_style)->default_value(OPT_STR_BW_GRAPH_DEFVAL), (string("write search graph at each iteration:\n")
			+ "- " + '"' + OPT_STR_BW_GRAPH_OPT_none + '"' + ", \n"
			+ "- " + '"' + OPT_STR_BW_GRAPH_OPT_TIKZ + '"'	+ ", or \n"
			+ "- " + '"' + OPT_STR_BW_GRAPH_OPT_DOTTY + '"'
			).c_str())
			(OPT_STR_OUTPUT_FILE, value<string>(&o)->default_value(OPT_STR_OUTPUT_FILE_DEFVAL), "monitor output file (\"stdout\" for console)")
#endif
			;

		//Problem instance
		options_description problem("Problem instance");
		problem.add_options()
			(OPT_STR_INPUT_FILE,	value<string>(&filename), "thread transition system (.tts file)")
			(OPT_STR_TARGET,		value<string>(&target_fn), "target state file (e.g. 1|0,1,1; don't specify for k-cover)")
			;

		//Initial state
		options_description istate("Initial state");
		istate.add_options()
			(OPT_STR_INI_SHARED,	value<shared_t>(&init_shared)->default_value(OPT_STR_INI_SHARED_DEFVAL), "initial shared state")
			(OPT_STR_INI_LOCAL,		value<local_t>(&init_local)->default_value(OPT_STR_INI_LOCAL_DEFVAL), "initial local state")
			(OPT_STR_SINGLE_INITIAL,bool_switch(&single_initial), "do not parametrize the local initial state")
			;

		//Exploration mode
		options_description exploration_mode("Exploration mode");
		exploration_mode.add_options()
			(OPT_STR_NO_POR,		bool_switch(&net.prj_all), "do not use partial order reduction")
			(OPT_STR_MODE,			value<string>(&mode_str)->default_value(CON_OPTION_STR), (string("exploration mode: \n") 
			+ "- " + '"' + CON_OPTION_STR + '"'		+ " (concurrent forward/backward), \n"
#ifndef PUBLIC_RELEASE
			+ "- " + '"' + FW_CON_OPTION_STR + '"'	+ " (forward, then concurrent forward/backward; for cross-check), \n"
			+ "- " + '"' + FWBW_OPTION_STR + '"'	+ " (forward, then backward; for cross-check), \n"
#endif
			+ "- " + '"' + FW_OPTION_STR + "'"		+ " (forward only), or \n" 
			+ "- " + '"' + BW_OPTION_STR + "'"		+ " (backward only)"
			).c_str());

		//FW options
		options_description fw_exploration("Forward exploration");
		fw_exploration.add_options()
			(OPT_STR_SATBOUND_FW,	value<int>(&kfw), "forward projection bound/bound for the k-cover")
			(OPT_STR_ACCEL_BOUND,	value<unsigned>(&ab)->default_value(OPT_STR_ACCEL_BOUND_DEFVAL), "acceleration bound")
			(OPT_STR_FW_INFO,		bool_switch(&print_fwinf), "print info during forward exploration")
			(OPT_STR_PRINT_HASH_INFO,	bool_switch(&print_hash_info), "print info during forward exploration (slow, req. stats option)")
			(OPT_STR_FW_ORDER,		value<string>(&forder_str)->default_value(OPT_STR_BW_ORDER_DEFVAL), (string("order of the forward workset:\n")
			+ "- " + '"' + ORDER_SMALLFIRST_OPTION_STR + '"'	+ " (small first), \n"
			+ "- " + '"' + ORDER_LARGEFIRST_OPTION_STR + '"'	+ " (large first), or \n"
			+ "- " + '"' + ORDER_RANDOM_OPTION_STR + '"'	+ " (random)"
			).c_str())
			(OPT_STR_FW_WEIGHT,		value<string>(&fweight_str)->default_value(OPT_STR_WEIGHT_DEFVAL), (string("weight for forward workset ordering:\n")
			+ "- " + '"' + OPT_STR_WEIGHT_DEPTH + '"'	+ " (depth), or \n"
			+ "- " + '"' + OPT_STR_WEIGHT_WIDTH + '"'	+ " (width)"
			).c_str())
			(OPT_STR_FW_WIDTH,	value<unsigned>(&max_fw_width)->default_value(OPT_STR_FW_WIDTH_DEFVAL), "maximum thread count considered during exploration")
			(OPT_STR_FW_THRESHOLD,	value<unsigned>(&fw_threshold)->default_value(OPT_STR_FW_THRESHOLD_DEFVAL), "stop oracle after that many seconds if it did not progress")
			;

		//BW options
		options_description bw_exploration("Backward exploration");
		bw_exploration.add_options()
			(OPT_STR_SATBOUND_BW,	value<unsigned>(&k)->default_value(2), "backward saturation bound/bound for the k-cover")
			("defer", bool_switch(&defer), "defer states with only non-global predecessor (optimistic exploration)")
			(OPT_STR_PRE_INFO,		bool_switch(&print_preinf), "print info during predecessor computation")
			(OPT_STR_BW_INFO,		bool_switch(&print_bwinf), "print info during backward exloration")
			(OPT_STR_BW_ORDER,		value<string>(&border_str)->default_value(OPT_STR_BW_ORDER_DEFVAL), (string("order of the backward workset:\n")
			+ "- " + '"' + ORDER_SMALLFIRST_OPTION_STR + '"'	+ " (small first), \n"
			+ "- " + '"' + ORDER_LARGEFIRST_OPTION_STR + '"'	+ " (large first), or \n"
			+ "- " + '"' + ORDER_RANDOM_OPTION_STR + '"'	+ " (random)"
			).c_str())
			(OPT_STR_BW_WEIGHT,		value<string>(&bweight_str)->default_value(OPT_STR_WEIGHT_DEFVAL), (string("weight for backward workset ordering:\n")
			+ "- " + '"' + OPT_STR_WEIGHT_DEPTH + '"'	+ " (depth), or \n"
			+ "- " + '"' + OPT_STR_WEIGHT_WIDTH + '"'	+ " (width)"
			).c_str())
			(OPT_STR_WRITE_BW_TRACE,bool_switch(&writetrace), "write exploration sequence to file (extension .trace)")
			(OPT_STR_READ_BW_TRACE,	bool_switch(&readtrace), "read exploration sequence from file (extension .trace)")
			;

		options_description cmdline_options;
		cmdline_options
			.add(misc)
			.add(problem)
			.add(istate)
			.add(exploration_mode)
#ifndef PUBLIC_RELEASE
			.add(fw_exploration)
			.add(bw_exploration)
#endif
			;

		positional_options_description p; p.add(OPT_STR_INPUT_FILE, -1);

		variables_map vm;
		store(command_line_parser(argc, argv).options(cmdline_options).positional(p).run(), vm), notify(vm);

		if(kfw == -1)
			kfw = k;

		if(h || v)
		{ 
			if(h) cout	<< "Usage: " << argv[0] << " [options] [" << OPT_STR_INPUT_FILE << "]" << endl << cmdline_options; 
			if(v) cout	<< "                                   \n" //http://patorjk.com/software/taag, font: Lean
				<< "    _/_/_/    _/_/_/_/    _/_/_/   \n"
				<< "   _/    _/  _/        _/          \n"
				<< "  _/_/_/    _/_/_/    _/           \n"
				<< " _/    _/  _/        _/            \n"
				<< "_/_/_/    _/          _/_/_/   v" << VERSION << "\n"
				<< "-----------------------------------\n"
				<< "                    Greedy Analysis\n"
				<< "         of Multi-Threaded Programs\n"
				<< "    with Non-Blocking Communication\n"
				<< "-----------------------------------\n"
				<< "     (c)2011 Alexander Kaiser      \n"
				<< " Oxford University, United Kingdom \n"
				<< "Build Date:  " << __DATE__ << " @ " << __TIME__
				;

			return EXIT_SUCCESS;
		}

		if(!stats_info){ statsout.rdbuf(0); } //OPT_STR_STATS

#ifndef WIN32
		if(to != 0)
		{
			rlimit t;
			if (getrlimit(RLIMIT_CPU, &t) < 0) throw logic_error("could not get softlimit for RLIMIT_CPU");
			t.rlim_cur = to;
			if(setrlimit(RLIMIT_CPU, &t) < 0) throw logic_error("could not set softlimit for RLIMIT_CPU");
			else cout << "successfully set CPU time limit" << endl;
		}

		if(mo != 0)
		{
			rlimit v;
			if (getrlimit(RLIMIT_AS, &v) < 0) throw logic_error("could not get softlimit for RLIMIT_AS");
			v.rlim_cur = (unsigned long)mo * 1024 * 1024; //is set in bytes rather than Mb
			if(setrlimit(RLIMIT_AS, &v) < 0) throw logic_error("could not set softlimit for RLIMIT_AS");
			else cout << "successfully set softlimit for the process's virtual memory" << endl;
		}
#endif

		(filename == string())?throw logic_error("No input file specified"):net.read_net_from_file(filename); //OPT_STR_INPUT_FILE

		if(BState::S == 0 || BState::L == 0) throw logic_error("Input file has invalid dimensions");

		if(target_fn != string())
		{
			try{ 
				net.target = new BState(target_fn,true); 
			}catch(...){
				ifstream target_in(target_fn.c_str());
				if(!target_in.good()) throw logic_error("cannot read from target input file");
				try{ string tmp; target_in >> tmp; net.target = new BState(tmp,true); }
				catch(...){ throw logic_error("invalid target file (only one target in the first line supported)"); }
			}

			if(!net.target->consistent()) throw logic_error("invalid target");
			net.Otarget = OState(net.target->shared,net.target->bounded_locals.begin(),net.target->bounded_locals.end());
			net.check_target = true;
		}

		net.init = OState(init_shared), net.local_init = init_local; //OPT_STR_INI_SHARED, OPT_STR_INI_LOCAL
		if(single_initial) 
			net.init.bounded_locals.insert(init_local);
		else
			net.init.unbounded_locals.insert(init_local);

#ifndef NOSPAWN_REWRITE
		if(net.has_spawns)
			net.init.unbounded_locals.insert(net.local_thread_pool);
#endif

		if      (mode_str == FW_OPTION_STR)		mode = FW, forward_projections = 0; //OPT_STR_MODE
		else if (mode_str == BW_OPTION_STR)		mode = BW;
		else if (mode_str == CON_OPTION_STR)	mode = CON, forward_projections = 1;
		else if (mode_str == FWBW_OPTION_STR)	mode = FWBW, forward_projections = 0;
		else if (mode_str == FW_CON_OPTION_STR)	mode = FW_CON, forward_projections = 1;
		else throw logic_error("Invalid mode"); 

		if(print_cover && (mode == CON || mode == FW_CON))
			cout << warning("printing cover information slows down concurrent mode") << endl;
		if(print_cover && (mode == FW))
			throw logic_error("printing cover information not supported in this mode");

		if(!print_fwinf){ fwinfo.rdbuf(0); } //OPT_STR_FW_INFO

		if(print_hash_info && !stats_info) throw logic_error("hash info requires stats argument");

		if     (forder_str == ORDER_SMALLFIRST_OPTION_STR) forder = unordered_priority_set<ostate_t>::less;
		else if(forder_str == ORDER_LARGEFIRST_OPTION_STR) forder = unordered_priority_set<ostate_t>::greater;
		else if(forder_str == ORDER_RANDOM_OPTION_STR) forder = unordered_priority_set<ostate_t>::random;
		else throw logic_error("invalid order argument");

		if     (fweight_str == OPT_STR_WEIGHT_DEPTH) fw_weight = order_depth;
		else if(fweight_str == OPT_STR_WEIGHT_WIDTH) fw_weight = order_width;
		else throw logic_error("invalid weight argument");

		if(!print_preinf){ preinf.rdbuf(0); } //OPT_STR_PRE_INFO
		if(!print_bwinf){ bout.rdbuf(0); } //OPT_STR_BW_INFO
		if(!debug_info){ dout.rdbuf(0); } //

		

		if	   (graph_style == OPT_STR_BW_GRAPH_OPT_none) graph_type = GTYPE_NONE;
		else if(graph_style == OPT_STR_BW_GRAPH_OPT_TIKZ) graph_type = GTYPE_TIKZ;
		else if(graph_style == OPT_STR_BW_GRAPH_OPT_DOTTY) graph_type = GTYPE_DOT;
		else throw logic_error("invalid graph type");

		if     (border_str == ORDER_SMALLFIRST_OPTION_STR) border = unordered_priority_set<bstate_t>::less;
		else if(border_str == ORDER_LARGEFIRST_OPTION_STR) border = unordered_priority_set<bstate_t>::greater;
		else if(border_str == ORDER_RANDOM_OPTION_STR) border = unordered_priority_set<bstate_t>::random;
		else throw logic_error("invalid order argument");

		if     (bweight_str == OPT_STR_WEIGHT_DEPTH) bw_weight = order_depth;
		else if(bweight_str == OPT_STR_WEIGHT_WIDTH) bw_weight = order_width;
		else throw logic_error("invalid weight argument");

		if(mode == CON && (writetrace || readtrace)) throw logic_error("reading/writing traces not supported in concurrent mode");
		else if(writetrace && readtrace) throw logic_error("cannot read and write trace at the same time"); //OPT_STR_WRITE_BW_TRACE, OPT_STR_READ_BW_TRACE
		else if(readtrace) work_sequence = read_trace(net.filename + ".trace");
		else if(writetrace) {
			ofstream w((net.filename + ".trace").c_str()); //empty trace file if it exists
			w << BState::S << " " << BState::L << " " << k << endl;
			w.close();
		}

		Net::net_stats_t sts = net.get_net_stats();
		statsout << "---------------------------------" << endl;
		statsout << "Statistics for " << net.filename << endl;
		statsout << "---------------------------------" << endl;
		statsout << "local states                    : " << sts.L << endl;
		statsout << "shared states                   : " << sts.S << endl;
		statsout << "transitions                     : " << sts.T << endl;
		statsout << "thread transitions              : " << sts.trans_type_counters[thread_transition] << endl;
		//statsout << "thread transitions (%)          : " << (unsigned)((float)(100*sts.trans_type_counters[thread_transition])/sts.T) << endl;
		statsout << "broadcast transitions           : " << sts.trans_type_counters[transfer_transition] << endl;
		//statsout << "broadcast transitions (%)       : " << (unsigned)((float)(100*sts.trans_type_counters[transfer_transition])/sts.T) << endl;
		statsout << "spawn transitions               : " << sts.trans_type_counters[spawn_transition] << endl;
		//statsout << "spawn transitions (%)           : " << (unsigned)((float)(100*sts.trans_type_counters[spawn_transition])/sts.T) << endl;
		statsout << "horizontal transitions (total)  : " << sts.trans_dir_counters[hor] << endl;
		//statsout << "horizontal transitions (%)      : " << (unsigned)((float)(100*sts.trans_dir_counters[hor])/sts.T) << endl;
		statsout << "non-horizontal trans. (total)   : " << sts.trans_dir_counters[nonhor] << endl;
		//statsout << "non-horizontal trans. (%)       : " << (unsigned)((float)(100*sts.trans_dir_counters[nonhor])/sts.T) << endl;
		statsout << "disconnected thread states      : " << sts.discond << endl;
		//statsout << "connected thread states (%)     : " << (unsigned)((float)(100*((sts.S * sts.L) - sts.discond)/(sts.S * sts.L))) << endl;
#ifdef DETAILED_TTS_STATS
		statsout << "strongly connected components   : " << sts.SCC_num << endl;
		statsout << "SCC (no disc. thread states)    : " << sts.SCC_num - sts.discond << endl;
#endif
		statsout << "maximum indegree                : " << sts.max_indegree << endl;
		statsout << "maximum outdegree               : " << sts.max_outdegree << endl;
		statsout << "maximum degree                  : " << sts.max_degree << endl;
		statsout << "target state                    : " << ((net.target==nullptr)?("none"):(net.target->str_latex())) << endl;
		statsout << "dimension of target state       : " << ((net.target==nullptr)?("invalid"):(boost::lexical_cast<string>(net.target->bounded_locals.size()))) << endl;
		statsout << "---------------------------------" << endl;

	}
	catch(std::exception& e)			{ cout << "INPUT ERROR: " << e.what() << "\n"; cout << "type " << argv[0] << " -h for instructions" << endl; return EXIT_FAILURE; }
	catch (...)                         { cerr << "unknown error while checking program arguments" << endl; return EXIT_FAILURE; }

#ifndef WIN32
	installSignalHandler();
#endif

	if(!net.check_target)	cout << "Problem to solve: Compute the " << k << "-cover" << endl;
	else					cout << "Problem to solve: Check specific target property" << endl;
	cout << "Running coverability engine..." << endl;

	ptime start_time;

	try
	{
		
#ifndef WIN32
		boost::thread mon_mem(monitor_mem_usage, mon_interval);
#endif

		//BW data structure
		complement_vec		U(k, BState::S, BState::L); //note: this causes a memory leak
		lowerset_vec		D(k, BState::S, net.prj_all);

		if(mode != CON)
		{

			//FW data structure
			if(mode == FW || mode == FWBW) //run fw
			{
				D.project_and_insert(net.init,shared_cmb_deque,false);
				start_time = microsec_clock::local_time();
				boost::thread bw_mem(print_bw_stats, mon_interval, o);
				do_fw_bfs(&net, ab, &D, &shared_cmb_deque, forward_projections, forder), shared_fw_done = 0;
				cout << "total time (fw): " << time_diff_str(finish_time,start_time) << endl;
			}

			if(mode == BW || mode == FWBW) //run bw
			{
				start_time = microsec_clock::local_time();
				boost::thread bw_mem(print_bw_stats, mon_interval, o);
				Pre2(&net,k,border,&U,work_sequence,print_cover), shared_bw_done = 0;
				cout << "total time (bw): " << time_diff_str(finish_time,start_time) << endl;
			}

			//compare results
			if(cross_check && mode == FWBW)
			{

				if(!net.check_target)
				{
					for(unsigned s = 0; s < BState::S; ++s)
					{
						if(D.lv[s].size() != U.luv[s].l_nodes.size()) throw logic_error("inconclusive k-cover (reported sizes differ)");
						foreach(const cmb_node_p c, D.lv[s]) if(U.luv[s].l_nodes.find(c) == U.luv[s].l_nodes.end()) throw logic_error("backward search unsound");
						foreach(const cmb_node_p c, U.luv[s].l_nodes) if(D.lv[s].find(c) == D.lv[s].end()) throw logic_error("forward search unsound");
					}
					cout << info("sequential forward/backward search results match (same k-cover)") << endl;
				}
				else if(fw_safe != bw_safe)
					throw logic_error("inconclusive safety result");
				else
					cout << info("sequential forward/backward search results match (same result for target)") << endl;
			}

		}

		if(mode == CON || mode == FW_CON)
		{
			//data structure shared by bw and fw
			bool fw_safe_seq;
			fw_safe_seq = fw_safe, fw_safe = true, bw_safe = true; //save old results for later comparison, and reset others
			shared_fw_done = false, shared_bw_done = false;

			shared_fw_finised_first = 0, shared_bw_finised_first = 0;

			//clean up data structures
			if(1 || mode == BW || mode == FW || mode == FWBW) //todo: remove first 1
			{
				D.clear();
				D.project_and_insert(net.init,shared_cmb_deque,false);
				U.clear();
			}

			//start and join threads 
			start_time = microsec_clock::local_time();
			
			boost::thread 
				fw(do_fw_bfs, &net, ab, &D, &shared_cmb_deque, forward_projections, forder), 
				bw(Pre2,&net,k,border,&U,work_sequence,print_cover), 
				bw_mem(print_bw_stats, mon_interval, o)
				;
			
			dout << "waiting for fw..." << endl, fw.join(), dout << "fw joined" << endl;
			dout << "waiting for bw..." << endl, bw.join(), dout << "bw joined" << endl;
			bw_mem.interrupt();
			bw_mem.join();
			
			cout << "total time (conc): " << time_diff_str(finish_time,start_time) << endl;
			
			if(mode == FW_CON && cross_check)
			{
				if(net.check_target)
				{
					if(!implies(fw_safe && bw_safe, fw_safe_seq) || !implies(!fw_safe || !bw_safe, !fw_safe_seq)) 
						throw logic_error("inconclusive results (conc. vs. seq. forward)");
					else
						cout << info("sequential forward/concurrent forward/backward search results match (same result for target)") << endl;
				}
				else
					throw logic_error("cross-check in concurrent mode currently not supported");
			}
		}

#ifndef WIN32
		mon_mem.interrupt(), mon_mem.join();
		cout << "max. virt. memory usage (mb): " << max_mem_used << endl;
#endif

		//TODO: This is currently not correct if the forward exploration it stopped due to the width bound/time threshold
		if(execution_state == RUNNING)
		{
			if(net.check_target) 
			{
				if(bw_safe && fw_safe) return_value = EXIT_VERIFICATION_SUCCESSFUL, cout << "VERIFICATION SUCCESSFUL" << endl;
				else return_value = EXIT_VERIFICATION_FAILED, cout << "VERIFICATION FAILED" << endl;
			}
			else
			{
				cout << k << "-COVER SUCCESSFULLY COMPUTED" << endl;
				if(print_cover)
					cout << endl,
					cout << "---------------------------------" << endl,
					cout << k << " cover statistics:" << endl,
					cout << "---------------------------------" << endl,
#ifdef PRINT_KCOVER_ELEMENTS
					cout << "coverable                       : " << endl, U.print_upper_set(cout), cout << endl,
					cout << "uncoverable                     : " << endl, U.print_lower_set(cout), cout << endl,
#endif
					cout << "coverable elements (all)        : " << U.lower_size() << endl,
					cout <<	"uncoverable elements (min)      : " << U.upper_size() << endl,
					cout << "---------------------------------" << endl;
			}
		}

#ifndef WIN32
		statsout << "execution state: ";
		switch(execution_state){
		case RUNNING: statsout << "SUCCESSFUL" << endl; break;
		case TIMEOUT: statsout << "TIMEOUT" << endl; break;
		case MEMOUT: statsout << "MEMOUT" << endl; break;
		case UNKNOWN: statsout << "UNKNOWN" << endl; break;
		}
#endif

	}
	catch(std::exception& e)			{ 
		cout << e.what() << "\n"; 
		return EXIT_FAILURE; 
	}
	catch (...)                         { cerr << "unknown error" << endl; return EXIT_FAILURE; }

	return return_value;

}
