//#define PUBLIC_RELEASE
//#define IMMEDIATE_STOP_ON_INTERRUPT //deactive to allow stopping the oracle with CTRG-C

/*

known bugs: 
	- regression/hor_por_vs_03 failed sometimes (seg. fault)
	- sometimes hangs at "Forward trace to target covering state found..."

todo:
	- add time measurement for i) predecessor computation, ii) global minimal check, and iii) locally minimal check; print them in the statistics
	- if this might improve the performance significantly, implement a look-up hash table for the backward search and small elements (<3 threads) 

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

#define EXIT_VERIFICATION_SUCCESSFUL		0
#define EXIT_VERIFICATION_SUCCESSFUL_STR	"VERIFICATION SUCCESSFUL"

#define EXIT_VERIFICATION_FAILED			10
#define EXIT_VERIFICATION_FAILED_STR		"VERIFICATION FAILED"

#define EXIT_UNKNOWN_RESULT					EXIT_FAILURE
#define EXIT_UNKNOWN_RESULT_STR				"VERIFICATION UNKNOWN"

#define EXIT_KCOVERCOMPUTED_SUCCESSFUL		EXIT_VERIFICATION_SUCCESSFUL
#define EXIT_KCOVERCOMPUTED_SUCCESSFUL_STR	EXIT_VERIFICATION_SUCCESSFUL_STR

#define EXIT_ERROR_RESULT					EXIT_FAILURE
#define EXIT_ERROR_RESULT_STR				"ERROR"

#define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#if !defined WIN32 && GCC_VERSION < 40601
#error need g++ >= 4.6.1
#endif

//#define EAGER_ALLOC
#define VERSION "1.0"

#include <iostream>
#include <fstream>
using namespace std;

#include "types.h"

//ostream
FullExpressionAccumulator
	fw_log(cerr.rdbuf()), //optional fw info
	fw_stats(cerr.rdbuf()), //fw stats

	bw_log(cerr.rdbuf()), //optional bw info
	bw_stats(cerr.rdbuf()), //bw stats

	main_livestats(cerr.rdbuf()), 
	main_log(cerr.rdbuf()), 
	main_res(cout.rdbuf()), //result: (VERIFICATION SUCCEEDED / VERIFICATION FAILED / ERROR)
	main_inf(cout.rdbuf()), //trace
	main_tme(cout.rdbuf()) //time and memory info
	;

ofstream main_livestats_ofstream; //must have the same storage duration as main_livestats
streambuf* main_livestats_rdbuf_backup;

#include "user_assert.h"
#include "net.h"
#include "ostate.h"
#include "bstate.h"
#include "complement.h"

#include "options_str.h"
unsigned max_fw_width = OPT_STR_FW_WIDTH_DEFVAL;
unsigned fw_threshold = OPT_STR_FW_THRESHOLD_DEFVAL;
bool resolve_omegas = false;
bool threshold_reached = false;
#include "bfc.interrupts.h"

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

unsigned fw_prj_found = 0,
	fw_last_prj_width = 0
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

#define TIMING

#ifdef TIMING
#define time_and_exe(e, duration) ptime start = microsec_clock::local_time(); e; duration += microsec_clock::local_time() - start
#else
#define time_and_exe(e, duration) e
#endif

bool fw_state_blocked = false;
int kfw = -1;
#include "fw.h"
bool defer = false;
bool fw_blocks_bw = false;
#include "bw.h"

void print_bw_stats(unsigned sleep_msec)
{
	if(sleep_msec == 0) return;

	const unsigned sdst = 5;
	const unsigned ldst = 7;
	const string sep = ",";

	main_livestats
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
		<< "FWPrj__" << sep
#ifndef WIN32
		<< "MemMB__" << sep//ldist 17
#endif
		<< "\n"
		;

	while(1) 
	{

		if(shared_bw_done || shared_fw_done)
			break;

		main_livestats 
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
			<< setw(ldst) << fw_prj_found << /*' ' << */sep
			//both
#ifndef WIN32
			<< setw(ldst) << memUsed()/1024/1024 << /*' ' << */sep
#endif
			<< "\n"
			, 
			main_livestats.flush()
			;

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
	else main_log << "trace input file " << (net.filename + ".trace") << " successfully opened" << "\n";

	unsigned s_,l_,k_;
	w >> s_ >> l_ >> k_;
	if(!(s_ == BState::S && l_ == BState::L)) throw logic_error("trace has invalid dimensions");

	stringstream in;
	{
		string line;
		while (getline(w, line = "")) 
		{
			const size_t comment_start = line.find("#");
			in << ( comment_start == string::npos ? line : line.substr(0, comment_start) ) << "\n"; 
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

	int return_value = EXIT_SUCCESS;

	srand((unsigned)microsec_clock::local_time().time_of_day().total_microseconds());

	enum {FW,BW,FWBW,CON,FW_CON} mode;
	unsigned k(OPT_STR_SATBOUND_BW_DEFVAL),ab(OPT_STR_ACCEL_BOUND_DEFVAL), mon_interval(OPT_STR_MON_INTERV_DEFVAL);
	unordered_priority_set<bstate_t>::order_t border;
	unordered_priority_set<ostate_t>::order_t forder;
	vector<BState> work_sequence;
	bool forward_projections(0), cross_check(0), print_cover(0);
	string o = OPT_STR_OUTPUT_FILE_DEFVAL;

	try {
		shared_t init_shared;
		local_t init_local, init_local2(-1);
		string filename, target_fn, border_str = OPT_STR_BW_ORDER_DEFVAL, forder_str = OPT_STR_FW_ORDER_DEFVAL, bweight_str = OPT_STR_WEIGHT_DEFVAL, fweight_str = OPT_STR_WEIGHT_DEFVAL, mode_str, graph_style = OPT_STR_BW_GRAPH_DEFVAL, tree_style = OPT_STR_BW_GRAPH_DEFVAL;
		bool h(0), v(0), ignore_target(0), print_sgraph(0), stats_info(0), print_bwinf(0), print_fwinf(0), single_initial(0), nomain_info(0), noresult_info(0), noressource_info(0), no_main_log(0);
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
			(OPT_STR_PRINT_KCOVER,	bool_switch(&print_cover), "print k-cover elements")
#endif
			(OPT_STR_STATS,			bool_switch(&stats_info), "print final statistics")
#ifndef PUBLIC_RELEASE
			(OPT_STR_MON_INTERV,	value<unsigned>(&mon_interval)->default_value(OPT_STR_MON_INTERV_DEFVAL), "update interval for on-the-fly statistics (ms, \"0\" for none)")
			(OPT_STR_NOMAIN_INFO,	bool_switch(&nomain_info), "print no main info")
			(OPT_STR_NORES_INFO,	bool_switch(&noresult_info), "print no result")
			(OPT_STR_NORESSOURCE_INFO,	bool_switch(&noressource_info), "print no ressource info")
			(OPT_STR_NOMAIN_LOG,	bool_switch(&no_main_log), "print no main log")
#endif
#ifndef WIN32
			(OPT_STR_TIMEOUT,		value<unsigned>(&to)->default_value(0), "CPU time in seconds (\"0\" for no limit)")
			(OPT_STR_MEMOUT,		value<unsigned>(&mo)->default_value(0), "virtual memory limit in megabyte (\"0\" for no limit)")
#endif
#ifndef PUBLIC_RELEASE
			(OPT_STR_BW_GRAPH,		value<string>(&graph_style)->default_value(OPT_STR_BW_GRAPH_DEFVAL), (string("write bw search graph:\n")
			+ "- " + '"' + OPT_STR_BW_GRAPH_OPT_none + '"' + ", \n"
			+ "- " + '"' + OPT_STR_BW_GRAPH_OPT_TIKZ + '"'	+ ", or \n"
			+ "- " + '"' + OPT_STR_BW_GRAPH_OPT_DOTTY + '"'
			).c_str())
			(OPT_STR_FW_TREE,		value<string>(&tree_style)->default_value(OPT_STR_BW_GRAPH_DEFVAL), ("write fw search tree (same arguments as above)"))
			(OPT_STR_OUTPUT_FILE,	value<string>(&o)->default_value(OPT_STR_OUTPUT_FILE_DEFVAL), "monitor output file (\"stdout\" for console)")
#endif
			;

		//Problem instance
		options_description problem("Problem instance");
		problem.add_options()
			(OPT_STR_INPUT_FILE,	value<string>(&filename), "thread transition system (.tts file)")
			(OPT_STR_TARGET,		value<string>(&target_fn), "target state file (e.g. 1|0,1,1)")
			(OPT_STR_IGN_TARGET,	bool_switch(&ignore_target), "ignore the target")
			;

		//Initial state
		options_description istate("Initial state");
		istate.add_options()
			(OPT_STR_INI_SHARED,	value<shared_t>(&init_shared)->default_value(OPT_STR_INI_SHARED_DEFVAL), "initial shared state")
			(OPT_STR_INI_LOCAL,		value<local_t>(&init_local)->default_value(OPT_STR_INI_LOCAL_DEFVAL), "initial local state")
			(OPT_STR_INI_LOCAL2,	value<local_t>(&init_local2), "second initial local state")
			(OPT_STR_SINGLE_INITIAL,bool_switch(&single_initial), "do not parametrize the local initial state")
			;

		//Exploration mode
		options_description exploration_mode("Exploration mode");
		exploration_mode.add_options()
#ifndef PUBLIC_RELEASE
			(OPT_STR_NO_POR,		bool_switch(&net.prj_all), "do not use partial order reduction")
#endif
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
			(OPT_STR_FW_NO_OMEGAS,	bool_switch(&resolve_omegas), "no omegas in successors (i.e. finite-state exploration)")
			;

		//BW options
		options_description bw_exploration("Backward exploration");
		bw_exploration.add_options()
			(OPT_STR_SATBOUND_BW,	value<unsigned>(&k)->default_value(OPT_STR_SATBOUND_BW_DEFVAL), "backward saturation bound/bound for the k-cover")
			("defer", bool_switch(&defer), "defer states with only non-global predecessors (optimistic exploration)")
			(OPT_STR_BW_INFO,		bool_switch(&print_bwinf), "print info during backward exloration")
			(OPT_STR_BW_WAITFORFW,	bool_switch(&fw_blocks_bw), "backward exloration is blocked until foward exploration reaches the threshold")
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
			if(h) main_inf	<< "Usage: " << argv[0] << " [options] [" << OPT_STR_INPUT_FILE << "]" << "\n" << cmdline_options; 
			if(v) main_inf	<< "                                   \n" //http://patorjk.com/software/taag, font: Lean
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
				<< "     (c)2012 Alexander Kaiser      \n"
				<< " Oxford University, United Kingdom \n"
				<< "Build Date:  " << __DATE__ << " @ " << __TIME__ << "\n";
				;

			return EXIT_SUCCESS;
		}

		if(!stats_info){ bw_stats.rdbuf(0), fw_stats.rdbuf(0); } //OPT_STR_STATS

		if(!print_fwinf){ fw_log.rdbuf(0); } //OPT_STR_FW_INFO
		
		if(!print_bwinf){ bw_log.rdbuf(0); } //OPT_STR_BW_INFO

		if(nomain_info){ main_inf.rdbuf(0); }
		else{ main_log << "Log: " << "\n"; } 

		if(noresult_info){ main_res.rdbuf(0); }
		
		if(noressource_info){ main_tme.rdbuf(0); }

		if(no_main_log){ main_log.rdbuf(0); }

#ifndef WIN32
		if(to != 0)
		{
			rlimit t;
			if (getrlimit(RLIMIT_CPU, &t) < 0) throw logic_error("could not get softlimit for RLIMIT_CPU");
			t.rlim_cur = to;
			if(setrlimit(RLIMIT_CPU, &t) < 0) throw logic_error("could not set softlimit for RLIMIT_CPU");
			else main_log << "successfully set CPU time limit" << "\n";
		}

		if(mo != 0)
		{
			rlimit v;
			if (getrlimit(RLIMIT_AS, &v) < 0) throw logic_error("could not get softlimit for RLIMIT_AS");
			v.rlim_cur = (unsigned long)mo * 1024 * 1024; //is set in bytes rather than Mb
			if(setrlimit(RLIMIT_AS, &v) < 0) throw logic_error("could not set softlimit for RLIMIT_AS");
			else main_log << "successfully set softlimit for the process's virtual memory" << "\n";
		}
#endif

		//(filename == string())?throw logic_error("No input file specified"):net.read_net_from_file(filename); //OPT_STR_INPUT_FILE
		net.read_net_from_file(filename);

		if(BState::S == 0 || BState::L == 0) throw logic_error("Input file has invalid dimensions");

		if(ignore_target)
			target_fn = "";

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

			main_log << "Problem to solve: check target" << "\n"; 
		}
		else
		{
#if 0
			throw logic_error("no target specified");
#endif 
			main_log << "Problem to solve: compute cover" << "\n"; 
		}

		net.init = OState(init_shared), net.local_init = init_local, net.init_local2 = init_local2; //OPT_STR_INI_SHARED, OPT_STR_INI_LOCAL
		if(single_initial) 
		{
			if(init_local2 != -1)
				net.init.bounded_locals.insert(init_local2);
			net.init.bounded_locals.insert(init_local);
		}
		else
		{
			if(init_local2 != -1)
				net.init.unbounded_locals.insert(init_local2);
			net.init.unbounded_locals.insert(init_local);
		}

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
			main_log << warning("printing cover information slows down concurrent mode") << "\n";
		if(print_cover && (mode == FW))
			throw logic_error("printing cover information not supported in this mode");

		if(print_hash_info && !stats_info) throw logic_error("hash info requires stats argument");

		if     (forder_str == ORDER_SMALLFIRST_OPTION_STR) forder = unordered_priority_set<ostate_t>::less;
		else if(forder_str == ORDER_LARGEFIRST_OPTION_STR) forder = unordered_priority_set<ostate_t>::greater;
		else if(forder_str == ORDER_RANDOM_OPTION_STR) forder = unordered_priority_set<ostate_t>::random;
		else throw logic_error("invalid order argument");

		if     (fweight_str == OPT_STR_WEIGHT_DEPTH) fw_weight = order_depth;
		else if(fweight_str == OPT_STR_WEIGHT_WIDTH) fw_weight = order_width;
		else throw logic_error("invalid weight argument");		

		if(resolve_omegas && max_fw_width == 0)
			throw logic_error((string(OPT_STR_FW_NO_OMEGAS) + " requires an argument for " + OPT_STR_FW_WIDTH).c_str());

		if(resolve_omegas)
		{
			main_log << "deactive acceleration, due to option " << OPT_STR_FW_NO_OMEGAS << "\n";
			ab = 0;
		}

		if	   (graph_style == OPT_STR_BW_GRAPH_OPT_none) graph_type = GTYPE_NONE;
		else if(graph_style == OPT_STR_BW_GRAPH_OPT_TIKZ) graph_type = GTYPE_TIKZ;
		else if(graph_style == OPT_STR_BW_GRAPH_OPT_DOTTY) graph_type = GTYPE_DOT;
		else throw logic_error("invalid graph type");

		if	   (tree_style == OPT_STR_BW_GRAPH_OPT_none) tree_type = GTYPE_NONE;
		else if(tree_style == OPT_STR_BW_GRAPH_OPT_TIKZ) tree_type = GTYPE_TIKZ;
		else if(tree_style == OPT_STR_BW_GRAPH_OPT_DOTTY) tree_type = GTYPE_DOT;
		else throw logic_error("invalid tree type");

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
			w << BState::S << " " << BState::L << " " << k << "\n";
			w.close();
		}

		Net::net_stats_t sts = net.get_net_stats();
		main_log << "---------------------------------" << "\n";
		main_log << "Statistics for " << net.filename << "\n";
		main_log << "---------------------------------" << "\n";
		main_log << "local states                    : " << sts.L << "\n";
		main_log << "shared states                   : " << sts.S << "\n";
		main_log << "transitions                     : " << sts.T << "\n";
		main_log << "thread transitions              : " << sts.trans_type_counters[thread_transition] << "\n";
		//main_log << "thread transitions (%)          : " << (unsigned)((float)(100*sts.trans_type_counters[thread_transition])/sts.T) << "\n";
		main_log << "broadcast transitions           : " << sts.trans_type_counters[transfer_transition] << "\n";
		//main_log << "broadcast transitions (%)       : " << (unsigned)((float)(100*sts.trans_type_counters[transfer_transition])/sts.T) << "\n";
		main_log << "spawn transitions               : " << sts.trans_type_counters[spawn_transition] << "\n";
		//main_log << "spawn transitions (%)           : " << (unsigned)((float)(100*sts.trans_type_counters[spawn_transition])/sts.T) << "\n";
		main_log << "horizontal transitions (total)  : " << sts.trans_dir_counters[hor] << "\n";
		//main_log << "horizontal transitions (%)      : " << (unsigned)((float)(100*sts.trans_dir_counters[hor])/sts.T) << "\n";
		main_log << "non-horizontal trans. (total)   : " << sts.trans_dir_counters[nonhor] << "\n";
		//main_log << "non-horizontal trans. (%)       : " << (unsigned)((float)(100*sts.trans_dir_counters[nonhor])/sts.T) << "\n";
		main_log << "disconnected thread states      : " << sts.discond << "\n";
		//main_log << "connected thread states (%)     : " << (unsigned)((float)(100*((sts.S * sts.L) - sts.discond)/(sts.S * sts.L))) << "\n";
#ifdef DETAILED_TTS_STATS
		main_log << "strongly connected components   : " << sts.SCC_num << "\n";
		main_log << "SCC (no disc. thread states)    : " << sts.SCC_num - sts.discond << "\n";
#endif
		main_log << "maximum indegree                : " << sts.max_indegree << "\n";
		main_log << "maximum outdegree               : " << sts.max_outdegree << "\n";
		main_log << "maximum degree                  : " << sts.max_degree << "\n";
		main_log << "target state                    : " << ((net.target==nullptr)?("none"):(net.target->str_latex())) << "\n";
		main_log << "dimension of target state       : " << ((net.target==nullptr)?("invalid"):(boost::lexical_cast<string>(net.target->bounded_locals.size()))) << "\n";
		main_log << "total core shared states        : " << sts.core_shared_states << "\n";
		main_log << "---------------------------------" << "\n";
		main_log.flush();

	}
	catch(std::exception& e)			{ cerr << "INPUT ERROR: " << e.what() << "\n"; main_res << "type " << argv[0] << " -h for instructions" << "\n"; return EXIT_FAILURE; }
	catch (...)                         { cerr << "unknown error while checking program arguments" << "\n"; return EXIT_FAILURE; }

#ifndef WIN32
	installSignalHandler();
#endif

	main_log << "Running coverability engine..." << "\n";

	//write stream "headers"
	bw_stats << "BW stats:" << "\n", fw_stats << "FW stats:" << "\n";
	fw_log << "FW log:" << "\n";
	bw_log << "BW log:" << "\n";
	main_inf << "Additional info:" << "\n";
	main_res << "Result: " << "\n";
	main_tme << "Ressources: " << "\n";

	ptime start_time;

	try
	{
		
		if(o != OPT_STR_OUTPUT_FILE_STDOUT)
		{
			main_livestats_ofstream.open(o.c_str()), main_log << "csv output file opened" << "\n";
			if(!main_livestats_ofstream.good()) 
				throw std::runtime_error((string("cannot write to file ") + o).c_str());

			main_livestats_rdbuf_backup = main_livestats.rdbuf(main_livestats_ofstream.rdbuf());
		}

#ifndef WIN32
		boost::thread mon_mem(monitor_mem_usage, mon_interval);
#endif

		//BW data structure

		main_log << "initializing complement data structure..." << "\n";
		complement_vec		U(k, 0, BState::L); //complement_vec		U(k, BState::S, BState::L);
		U.S = BState::S;
		auto x = complement_set(0,0);
		for(unsigned i = 0; i < BState::S; ++i)
			if(net.core_shared(i)) U.luv.push_back(move(complement_set(BState::L,k)));
			else U.luv.push_back(move(x));
		main_log << "done" << "\n";
		
		main_log << "initializing lowerset data structure..." << "\n";
		lowerset_vec		D(k, BState::S, net.prj_all);
		main_log << "done" << "\n";

		if(mode != CON)
		{

			//FW data structure
			if(mode == FW || mode == FWBW) //run fw
			{
				shared_fw_done = 0;
				D.project_and_insert(net.init,shared_cmb_deque,false);
				start_time = microsec_clock::local_time();
				boost::thread bw_mem(print_bw_stats, mon_interval);
				do_fw_bfs(&net, ab, &D, &shared_cmb_deque, forward_projections, forder);
				main_tme << "total time (fw): " << time_diff_str(finish_time,start_time) << "\n";
			}

			if(mode == BW || mode == FWBW) //run bw
			{
				shared_fw_done = 0;
				start_time = microsec_clock::local_time();
				boost::thread bw_mem(print_bw_stats, mon_interval);
				Pre2(&net,k,border,&U,work_sequence,print_cover);
				main_tme << "total time (bw): " << time_diff_str(finish_time,start_time) << "\n";
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
					main_log << info("sequential forward/backward search results match (same k-cover)") << "\n";
				}
				else if(fw_safe != bw_safe)
					throw logic_error("inconclusive safety result");
				else
					main_log << info("sequential forward/backward search results match (same result for target)") << "\n";
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
				bw_mem(print_bw_stats, mon_interval)
				;
			
			main_log << "waiting for fw..." << "\n", main_log.flush(), fw.join(), main_log << "fw joined" << "\n", main_log.flush();
			main_log << "waiting for bw..." << "\n", main_log.flush(), bw.join(), main_log << "bw joined" << "\n", main_log.flush();
			bw_mem.interrupt();
			bw_mem.join();
			
			main_tme << "total time (conc): " << time_diff_str(finish_time,start_time) << "\n";
			
			if(mode == FW_CON && cross_check)
			{
				if(net.check_target)
				{
					if(!implies(fw_safe && bw_safe, fw_safe_seq) || !implies(!fw_safe || !bw_safe, !fw_safe_seq)) 
						throw logic_error("inconclusive results (conc. vs. seq. forward)");
					else
						main_log << info("sequential forward/concurrent forward/backward search results match (same result for target)") << "\n";
				}
				else
					throw logic_error("cross-check in concurrent mode currently not supported");
			}
		}
		
#ifndef WIN32
		mon_mem.interrupt(), mon_mem.join();
		main_tme << "max. virt. memory usage (mb): " << max_mem_used << "\n";
#endif

		if(!net.check_target && print_cover)
		{
#ifdef PRINT_KCOVER_ELEMENTS
			main_log << "coverable                       : " << "\n", U.print_upper_set(main_log), main_log << "\n";
			main_log << "uncoverable                     : " << "\n", U.print_lower_set(main_log), main_log << "\n";
#endif
			main_inf << "coverable elements (all)        : " << U.lower_size() << "\n";
			main_inf <<	"uncoverable elements (min)      : " << U.upper_size() << "\n";
		}

		//TODO??: This is currently not correct if the forward exploration it stopped due to the width bound/time threshold

		invariant(implies(fw_state_blocked,!shared_fw_done)); //"shared_fw_done" means "fw done and sound"

		if(execution_state == RUNNING)
		{
			if(!shared_fw_done && !shared_bw_done)
				return_value = EXIT_UNKNOWN_RESULT, main_res << EXIT_UNKNOWN_RESULT_STR << "\n";
			else if(!net.check_target)
				return_value = EXIT_KCOVERCOMPUTED_SUCCESSFUL, main_res << EXIT_KCOVERCOMPUTED_SUCCESSFUL_STR << "\n";
			else if(bw_safe && fw_safe) 
				return_value = EXIT_VERIFICATION_SUCCESSFUL, main_res << EXIT_VERIFICATION_SUCCESSFUL_STR << "\n";
			else 
				return_value = EXIT_VERIFICATION_FAILED, main_res << EXIT_VERIFICATION_FAILED_STR << "\n";
		}
		else
		{
			invariant(execution_state == TIMEOUT || execution_state == MEMOUT || execution_state == INTERRUPTED);
			return_value = EXIT_ERROR_RESULT, main_res << EXIT_ERROR_RESULT_STR << "\n";
		}

	}
	catch(std::exception& e){
		return_value = EXIT_FAILURE, main_res << e.what() << "\n";}
	catch (...){
		return_value = EXIT_FAILURE, main_res << "unknown exception" << "\n";}

	if(o != OPT_STR_OUTPUT_FILE_STDOUT)
		main_livestats.rdbuf(main_livestats_rdbuf_backup), main_livestats_ofstream.close(), main_log << "csv output file closed" << "\n"; //otherwise the stream might be closed before the destruction of main_livestats which causes an error
	
	//print stream in this order; all other streams are flushed on destruction
	main_res.flush();
	main_inf.flush();
	main_tme.flush();

	return return_value;

}
