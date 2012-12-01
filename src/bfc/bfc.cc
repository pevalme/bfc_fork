//#define PUBLIC_RELEASE
//#define IMMEDIATE_STOP_ON_INTERRUPT //deactive to allow stopping the oracle with CTRG-C
//#define TIMING
//#define EAGER_ALLOC
#define MINBW

#define VERSION "2.0"

//$(SolutionDir)\regression\abp_vs_sm

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

#define EXIT_UNKNOWN_RESULT					1
#define EXIT_UNKNOWN_RESULT_STR				"VERIFICATION UNKNOWN"

#define EXIT_KCOVERCOMPUTED_SUCCESSFUL		EXIT_VERIFICATION_SUCCESSFUL
#define EXIT_KCOVERCOMPUTED_SUCCESSFUL_STR	EXIT_VERIFICATION_SUCCESSFUL_STR

#define EXIT_TIMEOUT_RESULT					2
#define EXIT_TIMEOUT_RESULT_STR				"TIMEOUT"

#define EXIT_MEMOUT_RESULT					3
#define EXIT_MEMOUT_RESULT_STR				"MEMOUT"

#define EXIT_OTHER_RESULT					4
#define EXIT_OTHER_RESULT_STR				"ERROR"

#define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#if !defined WIN32 && GCC_VERSION < 40601
#error need g++ >= 4.6.1
#endif

#include <iostream>
#include <fstream>
using namespace std;

#include "types.h"

//ostream
ostream_sync
	fw_log(cerr.rdbuf()), //optional fw info
	fw_stats(cerr.rdbuf()), //fw stats
	main_livestats(cerr.rdbuf()), 
	main_log(cerr.rdbuf()), 
	main_res(cout.rdbuf()), //result: (VERIFICATION SUCCEEDED / VERIFICATION FAILED / ERROR)
	main_inf(cout.rdbuf()), //trace
	main_tme(cout.rdbuf()), //time and memory info
	main_cov(cout.rdbuf()) //state found to be coverable/uncoverable
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
	fw_last_prj_width = 0,
	fw_last_prj_states = 0
	;

shared_cmb_deque_t 
	shared_cmb_deque
	;

volatile bool 
	shared_fw_done(0),
	shared_bw_done(0),
	shared_fw_finised_first(0),
	shared_bw_finised_first(0),
	need_stats(0)
	;

bool 
	print_cover(0),
	bw_safe(1), //false, if bw found a viol. conf
	fw_safe(1) //false, if fw found a viol. conf
	;

enum weigth_t
{
	order_width,
	order_depth,
	order_random
} bw_weight, fw_weight;

#ifdef TIMING
#define time_and_exe(e, duration) ptime start = microsec_clock::local_time(); e; duration += microsec_clock::local_time() - start
#else
#define time_and_exe(e, duration) e
#endif

bool fw_state_blocked = false;
#include "fw.h"
bool defer = false;
bool fw_blocks_bw = false;
//#include "bw.h"
#include "minbw.h"

void print_bw_stats(unsigned sleep_msec)
{
	if(sleep_msec == 0) return;

	const unsigned sdst = 5;
	const unsigned ldst = 7;
	const string sep = ",";

	main_livestats << endl
		<< setw(sdst) << "BCDep" << sep
		<< setw(sdst) << "BCWid" << sep
		<< setw(ldst) << "BItrs" << sep
		<< setw(ldst) << "BPrun" << sep
		<< setw(ldst) << "BNode" << sep
		<< setw(ldst) << "WNode" << sep
		<< setw(sdst) << "FDept" << sep
		
		<< setw(sdst) << "FWidt" << sep
		<< setw(ldst) << "FItrs" << sep
		<< setw(ldst) << "FNode" << sep
		<< setw(ldst) << "FWNod" << sep
		<< setw(ldst) << "FWPrj" << sep
#ifndef WIN32
		<< setw(ldst) << "MemMB__" << sep//ldist 17
#endif
		<< endl;

	while(1) 
	{

		if(shared_bw_done || shared_fw_done)
			break;

		main_livestats 
			//bw
			<< setw(sdst) << ctr_cdp << sep
			<< setw(sdst) << ctr_csz << sep
			<< setw(ldst) << ctr_wit << sep
			<< setw(ldst) << ctr_pit << sep
			<< setw(ldst) << ctr_msz << sep
			<< setw(ldst) << ctr_wsz << sep
			//fw
			<< setw(sdst) << ctr_fw_maxdepth << sep
			<< setw(sdst) << ctr_fw_maxwidth << sep
			<< setw(ldst) << f_its << sep
			<< setw(ldst) << fw_qsz << sep
			<< setw(ldst) << fw_wsz << sep
			<< setw(ldst) << fw_prj_found << sep
#ifndef WIN32
			<< setw(ldst) << memUsed()/1024/1024 << /*' ' << */sep
#endif
			<< endl, 
			main_livestats.flush(),
			need_stats = 1;

		boost::this_thread::sleep(boost::posix_time::milliseconds(sleep_msec));
	}
}

float time_diff_float(boost::posix_time::ptime a, boost::posix_time::ptime b)
{
	return (((float)(a - b).total_microseconds())/1000000);
}

#include "antichain_comb.h"
#include <boost/assign/std/set.hpp>
using namespace boost::assign;

int main(int argc, char* argv[]) 
{

	int return_value = EXIT_SUCCESS;

	srand((unsigned)microsec_clock::local_time().time_of_day().total_microseconds());

	enum {FW,BW,FWBW,CON,FW_CON} mode;
	unsigned k(OPT_STR_SATBOUND_BW_DEFVAL),ab(OPT_STR_ACCEL_BOUND_DEFVAL), mon_interval(OPT_STR_MON_INTERV_DEFVAL);
	unordered_priority_set<bstate_t>::order_t border;
	unordered_priority_set<ostate_t>::order_t forder;
	vector<BState> work_sequence;
	bool forward_projections(0), local_sat(0);
	string o = OPT_STR_OUTPUT_FILE_DEFVAL;

	try {
		string filename, target_fn, init_fn, border_str = OPT_STR_BW_ORDER_DEFVAL, forder_str = OPT_STR_FW_ORDER_DEFVAL, bweight_str = OPT_STR_WEIGHT_DEFVAL, fweight_str = OPT_STR_WEIGHT_DEFVAL, mode_str, graph_style = OPT_STR_BW_GRAPH_DEFVAL, tree_style = OPT_STR_BW_GRAPH_DEFVAL;
		bool h(0), v(0), ignore_target(0), prj_all(0), print_sgraph(0), stats_info(0), print_bwinf(0), print_fwinf(0), single_initial(0), nomain_info(0), noresult_info(0), noressource_info(0), no_main_log(0), reduce_log(0), netstats(0);

#ifndef WIN32
		unsigned to,mo;
#endif

		//Misc
		options_description misc;
		misc.add_options()
			(OPT_STR_HELP,			bool_switch(&h), "produce help message and exit")
			(OPT_STR_VERSION,		bool_switch(&v), "print version info and exit")
#ifndef WIN32
			(OPT_STR_TIMEOUT,		value<unsigned>(&to)->default_value(0), "CPU time in seconds (\"0\" for no limit)")
			(OPT_STR_MEMOUT,		value<unsigned>(&mo)->default_value(0), "virtual memory limit in megabyte (\"0\" for no limit)")
#endif
			;

		options_description logging("Log output");
		logging.add_options()
			(OPT_STR_MON_INTERV,	value<unsigned>(&mon_interval)->default_value(OPT_STR_MON_INTERV_DEFVAL), "update interval for on-the-fly statistics (ms, \"0\" for none)")			
			(OPT_STR_NOMAIN_INFO,	bool_switch(&nomain_info), "print no main info")
			(OPT_STR_NORES_INFO,	bool_switch(&noresult_info), "print no result")
			(OPT_STR_NORESSOURCE_INFO,	bool_switch(&noressource_info), "print no ressource info")
			(OPT_STR_NOMAIN_LOG,	bool_switch(&no_main_log), "print no main log")
			(OPT_STR_REDUCE_LOG,	bool_switch(&reduce_log), "print no main log")
			(OPT_STR_FORCE_FLUSH,	bool_switch(&ostream_sync::force_flush), "force output to flush (thread output may interleave)")
			;

		options_description statistics("Statistics");
		statistics.add_options()
			(OPT_STR_BW_GRAPH,		value<string>(&graph_style)->default_value(OPT_STR_BW_GRAPH_DEFVAL), (string("write bw search graph:\n")
			+ "- " + '"' + OPT_STR_BW_GRAPH_OPT_none + '"' + ", \n"
			+ "- " + '"' + OPT_STR_BW_GRAPH_OPT_TIKZ + '"'	+ ", or \n"
			+ "- " + '"' + OPT_STR_BW_GRAPH_OPT_DOTTY + '"'
			).c_str())
			(OPT_STR_FW_TREE,		value<string>(&tree_style)->default_value(OPT_STR_BW_GRAPH_DEFVAL), ("write fw search tree (same arguments as above)"))
			(OPT_STR_OUTPUT_FILE,	value<string>(&o)->default_value(OPT_STR_OUTPUT_FILE_DEFVAL), "monitor output file (\"stdout\" for console)")

			(OPT_STR_STATS,			bool_switch(&stats_info), "print final statistics")
			(OPT_STR_NETSTATS,		bool_switch(&netstats), "compute and print net statistics")
			(OPT_STR_PRINT_KCOVER,	bool_switch(&print_cover), "print uncovered elements")
			;

		//Problem instance
		options_description problem("Problem instance");
		problem.add_options()
			(OPT_STR_INPUT_FILE,	value<string>(&filename), "thread transition system (.tts file)")
			(OPT_STR_TARGET,		value<string>(&target_fn), "target state file (e.g. 1|0,1,1)")
			(OPT_STR_INIT,			value<string>(&init_fn), "initial state file (e.g. 1|0 or 1|0,1/2)")
			(OPT_STR_IGN_TARGET,	bool_switch(&ignore_target), "ignore the target")
			;

		//Exploration mode
		options_description exploration_mode("Exploration mode");
		exploration_mode.add_options()
			(OPT_STR_NO_POR,		bool_switch(&prj_all), "do not use partial order reduction")
			(OPT_STR_MODE,			value<string>(&mode_str)->default_value(CON_OPTION_STR), (string("exploration mode: \n") 
			+ "- " + '"' + CON_OPTION_STR + '"'		+ " (concurrent forward/backward), \n"
			+ "- " + '"' + CON2_OPTION_STR+ '"'		+ " (concurrent forward/backward with pessimistic exploration), \n"
			+ "- " + '"' + FW_OPTION_STR  + "'"		+ " (forward only), or \n" 
			+ "- " + '"' + BC_OPTION_STR  + "'"		+ " (standard backward search), or \n" 
			+ "- " + '"' + BW_OPTION_STR  + "'"		+ " (backward only)"
			).c_str());

		//FW options
		options_description fw_exploration("Forward search");
		fw_exploration.add_options()
			(OPT_STR_ACCEL_BOUND,	value<unsigned>(&ab)->default_value(OPT_STR_ACCEL_BOUND_DEFVAL), "acceleration bound")
			(OPT_STR_FW_INFO,		bool_switch(&print_fwinf), "print info during forward exploration")
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
		options_description bw_exploration("Backward search");
		bw_exploration.add_options()
			(OPT_STR_SATBOUND_BW,	value<unsigned>(&k)->default_value(OPT_STR_SATBOUND_BW_DEFVAL), "backward saturation bound/bound for the k-cover")
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
			//(OPT_STR_LOCAL_SAT,		bool_switch(&local_sat), "only saturate with state consisting of multiple occurences of the same local")
			;

		options_description cmdline_options;
		cmdline_options
			.add(misc)
			.add(problem)
			.add(exploration_mode)
			.add(logging)
			.add(statistics)
#ifndef PUBLIC_RELEASE
			.add(fw_exploration)
			.add(bw_exploration)
#endif
			;

		positional_options_description p; p.add(OPT_STR_INPUT_FILE, -1);

		variables_map vm;
		store(command_line_parser(argc, argv).options(cmdline_options).positional(p).run(), vm), notify(vm);

		if(h || v)
		{ 
			if(h) main_inf	<< "Usage: " << argv[0] << " [options] [" << OPT_STR_INPUT_FILE << "]" << endl << cmdline_options; 
			if(v) main_inf	<< "                                   \n" //http://patorjk.com/software/taag, font: Lean
				<< "    _/_/_/    _/_/_/_/    _/_/_/   \n"
				<< "   _/    _/  _/        _/          \n"
				<< "  _/_/_/    _/_/_/    _/           \n"
				<< " _/    _/  _/        _/            \n"
				<< "_/_/_/    _/          _/_/_/   v" << VERSION << endl
				<< "-----------------------------------\n"
				<< "                    Greedy Analysis\n"
				<< "         of Multi-Threaded Programs\n"
				<< "    with Non-Blocking Communication\n"
				<< "-----------------------------------\n"
				<< "     (c)2012 Alexander Kaiser      \n"
				<< " Oxford University, United Kingdom \n"
				<< "Build Date:  " << __DATE__ << " @ " << __TIME__ << endl;
				;

			return EXIT_SUCCESS;
		}

		if(!stats_info){ bw_stats.rdbuf(0), fw_stats.rdbuf(0); } //OPT_STR_STATS

		if(!print_fwinf){ fw_log.rdbuf(0); } //OPT_STR_FW_INFO
		
		if(!print_bwinf){ bw_log.rdbuf(0); } //OPT_STR_BW_INFO

		if(nomain_info){ main_inf.rdbuf(0); }
		else{ main_log << "Log: " << endl; } 

		if(noresult_info){ main_res.rdbuf(0); }
		
		if(noressource_info){ main_tme.rdbuf(0); }

		if(no_main_log){ main_log.rdbuf(0); }

		if(!print_cover){ main_cov.rdbuf(0); }

		if(!reduce_log){ net.reduce_log.rdbuf(0); }

#ifndef WIN32
		if(to != 0)
		{
			rlimit t;
			if (getrlimit(RLIMIT_CPU, &t) < 0) throw logic_error("could not get softlimit for RLIMIT_CPU");
			t.rlim_cur = to;
			if(setrlimit(RLIMIT_CPU, &t) < 0) throw logic_error("could not set softlimit for RLIMIT_CPU");
			else main_log << "successfully set CPU time limit" << endl;
		}

		if(mo != 0)
		{
			rlimit v;
			if (getrlimit(RLIMIT_AS, &v) < 0) throw logic_error("could not get softlimit for RLIMIT_AS");
			v.rlim_cur = (unsigned long)mo * 1024 * 1024; //is set in bytes rather than Mb
			if(setrlimit(RLIMIT_AS, &v) < 0) throw logic_error("could not set softlimit for RLIMIT_AS");
			else main_log << "successfully set softlimit for the process's virtual memory" << endl;
		}
#endif

		if(ignore_target) target_fn = "";

		if(target_fn != string()) main_log << "Problem to solve: check target" << endl; 
		else main_log << "Problem to solve: compute cover" << endl; 

		//read problem instance
		Net(filename,target_fn,init_fn,prj_all).swap(net);

		if(!prj_all) net.reduce(prj_all);
		
		//cout << net << endl;

		BState::S = net.S, BState::L = net.L;

		invariant(implies(target_fn == string(),net.target.type = BState::invalid));
		if(target_fn != string() && !net.target.consistent()) throw logic_error("invalid target state");
		if(!net.init.consistent()) throw logic_error("invalid initial state");

		if      (mode_str == FW_OPTION_STR)		mode = FW, forward_projections = 0; //OPT_STR_MODE
		else if (mode_str == BW_OPTION_STR)
			mode = BW,
			defer = false;
		else if (mode_str == BC_OPTION_STR)		mode = BW, k = 0, main_log << info("saturation deactivated") << endl;
		else if (mode_str == CON_OPTION_STR)	
			mode = CON, 
			defer = true, //use defer-mode whenever fw is used, otherwise the memory consumption is too high
			forward_projections = 1;
		else if (mode_str == CON2_OPTION_STR) //defer deactivated (use for correct statistics)
			mode = CON, 
			defer = false,
			forward_projections = 1;
		else throw logic_error("Invalid mode"); 

		if(defer) main_log << info("performing optimistic backward exploration") << endl;
		else main_log << info("performing pessimistic backward exploration") << endl;

		if(print_cover && (mode == CON || mode == FW_CON))
			main_log << warning("printing cover information slows down concurrent mode") << endl;
		if(print_cover && (mode == FW))
			throw logic_error("printing cover information not supported in this mode");

		//TODO: remove comment out
		//if(k != 0 && mode == FW)
		//	k = 0, main_log << info("saturation deactivated") << endl;

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
			main_log << "deactive acceleration, due to option " << OPT_STR_FW_NO_OMEGAS << endl;
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

		if(netstats){
			Net::stats_t sts = net.get_stats(false);
			main_log << "---------------------------------" << endl;
			main_log << "Statistics for " << net.filename << endl;
			main_log << "---------------------------------" << endl;
			main_log << "local states                    : " << sts.L << endl;
			main_log << "shared states                   : " << sts.S << endl;
			main_log << "transitions                     : " << sts.T << endl;
			//main_log << "thread transitions              : " << sts.trans_type_counters[thread_transition] << endl;
			////main_log << "thread transitions (%)          : " << (unsigned)((float)(100*sts.trans_type_counters[thread_transition])/sts.T) << endl;
			//main_log << "broadcast transitions           : " << sts.trans_type_counters[transfer_transition] << endl;
			////main_log << "broadcast transitions (%)       : " << (unsigned)((float)(100*sts.trans_type_counters[transfer_transition])/sts.T) << endl;
			//main_log << "spawn transitions               : " << sts.trans_type_counters[spawn_transition] << endl;
			////main_log << "spawn transitions (%)           : " << (unsigned)((float)(100*sts.trans_type_counters[spawn_transition])/sts.T) << endl;
			//main_log << "horizontal transitions (total)  : " << sts.trans_dir_counters[hor] << endl;
			////main_log << "horizontal transitions (%)      : " << (unsigned)((float)(100*sts.trans_dir_counters[hor])/sts.T) << endl;
			//main_log << "non-horizontal trans. (total)   : " << sts.trans_dir_counters[nonhor] << endl;
			//main_log << "non-horizontal trans. (%)       : " << (unsigned)((float)(100*sts.trans_dir_counters[nonhor])/sts.T) << endl;
			main_log << "disconnected thread states      : " << sts.discond << endl;
			//main_log << "connected thread states (%)     : " << (unsigned)((float)(100*((sts.S * sts.L) - sts.discond)/(sts.S * sts.L))) << endl;
			main_log << "maximum indegree                : " << sts.max_indegree << endl;
			main_log << "maximum outdegree               : " << sts.max_outdegree << endl;
			main_log << "maximum degree                  : " << sts.max_degree << endl;
			main_log << "target state                    : " << ((net.target.type==BState::invalid)?(BState(0,0,BState::bot)):(net.target)) << endl;
			main_log << "dimension of target state       : " << ((net.target.type==BState::invalid)?("invalid"):(boost::lexical_cast<string>(net.target.bounded_locals.size()))) << endl;
			main_log << "total core shared states        : " << sts.core_shared_states << endl;
			main_log << "---------------------------------" << endl;
			main_log.flush();
		}

	}
	catch(std::exception& e)			{ cerr << "INPUT ERROR: " << e.what() << endl; main_res << "type " << argv[0] << " -h for instructions" << endl; return EXIT_FAILURE; }
	catch (...)                         { cerr << "unknown error while checking program arguments" << endl; return EXIT_FAILURE; }

#ifndef WIN32
	installSignalHandler();
#endif

	main_log << "Running coverability engine..." << endl;

	//write stream "headers"
	bw_stats << "BW stats:" << endl, fw_stats << "FW stats:" << endl;
	fw_log << "FW log:" << endl;
	bw_log << "BW log:" << endl;
	main_inf << "Additional info:" << endl;
	main_res << "Result: " << endl;
	main_tme << "Ressources: " << endl;
	main_cov << "Coverability info:" << endl;

	ptime start_time;

	try
	{
		
		if(o != OPT_STR_OUTPUT_FILE_STDOUT)
		{
			main_livestats_ofstream.open(o.c_str()), main_log << "csv output file opened" << endl;
			if(!main_livestats_ofstream.good()) 
				throw std::runtime_error((string("cannot write to file ") + o).c_str());

			main_livestats_rdbuf_backup = main_livestats.rdbuf(main_livestats_ofstream.rdbuf());
		}

#ifndef WIN32
		boost::thread mon_mem(monitor_mem_usage, mon_interval);
#endif

		//BW data structure

		main_log << "initializing complement data structure..." << endl;
		complement_vec		U(k, 0, BState::L,!local_sat); //complement_vec		U(k, BState::S, BState::L);
		U.S = BState::S;
		auto x = complement_set(0,0,!local_sat);
		for(unsigned i = 0; i < BState::S; ++i)
			if(net.core_shared[i]) U.luv.push_back(std::move(complement_set(BState::L,k,!local_sat)));
			else U.luv.push_back(std::move(x));
		
		main_log << "initializing lowerset data structure..." << endl;
		lowerset_vec		D(k, BState::S, !local_sat);
		main_log << "done" << endl;
		if(mode != CON)
		{

			//FW data structure
			if(mode == FW || mode == FWBW) //run fw
			{
				shared_fw_done = 0;
				start_time = microsec_clock::local_time();
				boost::thread bw_mem(print_bw_stats, mon_interval);
				do_fw_bfs(&net, ab, &D, &shared_cmb_deque, forward_projections, forder);
				finish_time = boost::posix_time::microsec_clock::local_time();
				main_tme << "total time (fw): "<< setprecision(2) << fixed << time_diff_float(finish_time,start_time) << endl;
			}

			if(mode == BW || mode == FWBW) //run bw
			{
				shared_fw_done = 0;
				start_time = microsec_clock::local_time();
				boost::thread bw_mem(print_bw_stats, mon_interval);
#ifdef MINBW
				main_log << "starting backward search..." << endl;
				minbw(&net,k,&U);
#else
				Pre2(&net,k,border,&U,work_sequence,print_cover);
#endif
				finish_time = boost::posix_time::microsec_clock::local_time();
				main_tme << "total time (bw): "<< setprecision(2) << fixed << time_diff_float(finish_time,start_time) << endl;
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
				U.clear();
			}

			//start and join threads
			start_time = microsec_clock::local_time();
			
			boost::thread 
				fw(do_fw_bfs, &net, ab, &D, &shared_cmb_deque, forward_projections, forder), 
#ifdef MINBW
				bw(minbw,&net,k,&U), 
#else
				bw(Pre2,&net,k,border,&U,work_sequence,print_cover), 
#endif
				bw_mem(print_bw_stats, mon_interval)
				;

			finish_time = boost::posix_time::microsec_clock::local_time();
			
			main_log << "waiting for fw..." << endl, main_log.flush(), fw.join(), main_log << "fw joined" << endl, main_log.flush();
			main_log << "waiting for bw..." << endl, main_log.flush(), bw.join(), main_log << "bw joined" << endl, main_log.flush();
			bw_mem.interrupt();
			bw_mem.join();
			
			main_tme << "total time (conc): "<< setprecision(2) << fixed << time_diff_float(finish_time,start_time) << endl;
		}
		
#ifndef WIN32
		mon_mem.interrupt(), mon_mem.join();
		main_tme << "max. virt. memory usage (mb): " << max_mem_used << endl;
#endif

		if(net.target.type == BState::invalid && print_cover)
		{
			for(unsigned i = 0; i < BState::S; ++i)
			{
				if(!net.core_shared[i]) continue;
				foreach(auto p, U.luv[i].u_nodes)
					main_cov << BState(i,p->c.begin(),p->c.end()) << endl;
			}

#ifdef PRINT_KCOVER_ELEMENTS
			main_cov << "coverable                       : " << endl, U.print_upper_set(main_log), main_log << endl;
			main_cov << "uncoverable                     : " << endl, U.print_lower_set(main_log), main_log << endl;
#endif
			main_cov << "coverable elements (all)        : " << U.lower_size() << endl;
			main_cov <<	"uncoverable elements (min)      : " << U.upper_size() << endl;
		}

		invariant(exe_state == RUNNING || exe_state == TIMEOUT || exe_state == MEMOUT || exe_state == INTERRUPTED);
		switch(exe_state)
		{
		case RUNNING:
			if(!shared_fw_done && !shared_bw_done)
				return_value = EXIT_UNKNOWN_RESULT, main_res << EXIT_UNKNOWN_RESULT_STR << endl;
			else if(net.target.type == BState::invalid)
				return_value = EXIT_KCOVERCOMPUTED_SUCCESSFUL, main_res << EXIT_KCOVERCOMPUTED_SUCCESSFUL_STR << endl;
			else if(bw_safe && fw_safe) 
				return_value = EXIT_VERIFICATION_SUCCESSFUL, main_res << EXIT_VERIFICATION_SUCCESSFUL_STR << endl;
			else 
				return_value = EXIT_VERIFICATION_FAILED, main_res << EXIT_VERIFICATION_FAILED_STR << endl;
			break;
		case TIMEOUT:
			return_value = EXIT_TIMEOUT_RESULT, main_res << EXIT_TIMEOUT_RESULT_STR << endl;
			break;
		case MEMOUT:
			return_value = EXIT_MEMOUT_RESULT, main_res << EXIT_MEMOUT_RESULT_STR << endl;
			break;
		case INTERRUPTED:
			return_value = EXIT_OTHER_RESULT, main_res << EXIT_OTHER_RESULT_STR << endl;
			break;
		}

	}
	catch(std::exception& e){
		return_value = EXIT_FAILURE, main_res << e.what() << endl;}
	catch (...){
		return_value = EXIT_FAILURE, main_res << "unknown exception" << endl;}

	if(o != OPT_STR_OUTPUT_FILE_STDOUT)
		main_livestats.rdbuf(main_livestats_rdbuf_backup), main_livestats_ofstream.close(), main_log << "csv output file closed" << endl; //otherwise the stream might be closed before the destruction of main_livestats which causes an error
	
	//print stream in this order; all other streams are flushed on destruction
	main_res.flush();
	main_inf.flush();
	main_tme.flush();

	return return_value;

}
