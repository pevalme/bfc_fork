#ifndef WIN32
#include <stdlib.h>
#include <csignal>
#include <errno.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <csignal>

void disable_mem_limit()
{
	static rlimit t;
	if (getrlimit(RLIMIT_AS, &t) < 0) 
		throw std::runtime_error("could not get softlimit for RLIMIT_AS");
	t.rlim_cur = RLIM_INFINITY;
	if(setrlimit(RLIMIT_AS, &t) < 0) 
		throw std::runtime_error("could not set softlimit for RLIMIT_AS");
}

void disable_time_limit()
{
	rlimit t;
	if (getrlimit(RLIMIT_CPU, &t) < 0) 
		throw std::runtime_error("could not get softlimit for RLIMIT_CPU");
	t.rlim_cur = RLIM_INFINITY;
	if(setrlimit(RLIMIT_CPU, &t) < 0) 
		throw std::runtime_error("could not set softlimit for RLIMIT_CPU");
}

#ifdef IMMEDIATE_STOP_ON_INTERRUPT
bool first_interrupt = false;
#else
bool first_interrupt = true;
#endif
void catcher(int sig)
{
	fflush(stdout);
	switch(sig)
	{
	case SIGXCPU:	main_log << "Interrupted by signal SIGXCPU" << "\n"; break;
	case SIGINT:	main_log << "Interrupted by signal SIGINT" << "\n"; break;
	}

	if(first_interrupt)
	{
		first_interrupt = false;
		main_log << "first interrupt" << "\n";
		switch(sig)
		{

		case SIGXCPU:
			disable_time_limit(); //disable soft limit
			exe_state = TIMEOUT;
			main_log << "time limit disabled; execution state changed to TIMEOUT" << "\n";
			break;

		case SIGINT: //fall-through
			//max_fw_width = 1;
			//main_log << "stopping oracle" << "\n";
			//break;

		default: 
			exe_state = INTERRUPTED;
			main_log << "execution state changed to INTERRUPTED" << "\n";
		}
		main_log.flush();
	}
	else
	{
		main_log << "second interrupt; terminate" << "\n";

		//fw_log.flush();
		//fw_stats.flush();

		//bw_log.flush();
		//bw_stats.flush();

		//main_livestats.flush();
		//main_log.flush();
		//main_res.flush();
		//main_inf.flush();
		//main_tme.flush();

		_exit(sig);
	}
}

void installSignalHandler()
{
	//install signal stack, otherwise SIGSEG cannot be handleded
	stack_t ss;
	const unsigned int stack_size = SIGSTKSZ*10;
	ss.ss_sp = malloc(stack_size);
	if (ss.ss_sp == NULL) 
		throw std::runtime_error("could not allocate signal stack context");
	ss.ss_size = stack_size, ss.ss_flags = 0;
	if (sigaltstack(&ss, NULL) == -1) 
		throw std::runtime_error("could install signal stack context");

	//declare act to deal with action on signal set.
	static struct sigaction act;
	act.sa_handler = catcher;
	act.sa_flags = SA_ONSTACK;
	sigfillset(&(act.sa_mask));

	// install signal handler
	if(sigaction( SIGXCPU, &act, NULL ) != 0)
		throw std::runtime_error("could not install SIGXCPU signal handler");
	if(sigaction( SIGINT, &act, NULL )  != 0)
		throw std::runtime_error("could not install SIGINT signal handler");
}

//from minisat
static inline int memReadStat(int field)
{
    int value = 0, r = 0;
    char    name[256];
    pid_t pid = getpid();
    sprintf(name, "/proc/%d/statm", pid);
    FILE*   in = fopen(name, "rb");
    if (in == NULL) return 0;
    for (; field >= 0 && r != EOF; field--)
        r = fscanf(in, "%d", &value);
    fclose(in);
    return r == EOF?0:value;
}

static inline long unsigned memUsed()
{ 
	return (long unsigned)memReadStat(0) * (long unsigned)getpagesize(); 
}

long unsigned max_mem_used = 0;
void monitor_mem_usage(unsigned sleep_msec)
{
	if(sleep_msec == 0) return;
	while(1) boost::this_thread::sleep(boost::posix_time::milliseconds(sleep_msec)), max_mem_used = max(max_mem_used,memUsed()/1024/1024);
}

#endif
