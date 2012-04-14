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
	if (getrlimit(RLIMIT_AS, &t) < 0) maincerr << ("could not get softlimit for RLIMIT_AS") << "\n";
	t.rlim_cur = RLIM_INFINITY;
	if(setrlimit(RLIMIT_AS, &t) < 0) maincerr << ("could not set softlimit for RLIMIT_AS") << "\n";
}

void disable_time_limit()
{
	rlimit t;
	if (getrlimit(RLIMIT_CPU, &t) < 0) maincerr << ("could not get softlimit for RLIMIT_CPU") << "\n";
	t.rlim_cur = RLIM_INFINITY;
	if(setrlimit(RLIMIT_CPU, &t) < 0) maincerr << ("could not set softlimit for RLIMIT_CPU") << "\n";
}

#ifdef IMMEDIATE_STOP_ON_INTERRUPT
bool first_interrupt = false;
#else
bool first_interrupt = true;
#endif
void catcher(int sig)
{
	fflush(stdout);
	maincerr << "Interrupted by signal ";
	switch(sig){
	case SIGXCPU:	maincerr << "SIGXCPU" << "\n"; break;
	//case SIGSEGV:	maincerr << "SIGSEGV" << "\n"; break; //this signal is not generated when the memory limit is reached; a malloc fails instead, causing std:bad_all to be thrown
	case SIGINT:	maincerr << "SIGINT" << "\n"; break;
	default:		maincerr << "unknown signal, terminate" << "\n"; _exit(7);
	}

	if(first_interrupt)
	{
		first_interrupt = false;
		maincerr << "first interrupt" << "\n";
		switch(sig)
		{
		case SIGXCPU:
			//disable soft limit
			disable_time_limit();
			execution_state = TIMEOUT;
			maincerr << "time limit disabled; execution state changed to TIMEOUT" << "\n";
			break;
		case SIGINT:
			max_fw_width = 1;
			maincerr << "stopping oracle" << "\n";
			break;
		default: 
			execution_state = UNKNOWN;
			maincerr << "execution state changed to UNKNOWN" << "\n";
		}
	}
	else
	{
		maincerr << "second interrupt; terminate" << "\n";
		_exit(5);
	}
}

void installSignalHandler()
{
	//install signal stack, otherwise SIGSEG cannot be handleded
	stack_t ss;
	const unsigned int stack_size = SIGSTKSZ*10;
	ss.ss_sp = malloc(stack_size);
	if (ss.ss_sp == NULL) { maincerr << "Could not allocate signal stack context" << "\n"; exit(3); }
	ss.ss_size = stack_size, ss.ss_flags = 0;
	if (sigaltstack(&ss, NULL) == -1) { maincerr << "Could install signal stack context" << "\n"; exit(3); }
	maincout << "Signal stack context installed" << "\n";

	//declare act to deal with action on signal set.
	static struct sigaction act;
	act.sa_handler = catcher;
	act.sa_flags = SA_ONSTACK; //act.sa_flags = 0;
	sigfillset(&(act.sa_mask));

	// install signal handler
	if(sigaction( SIGXCPU, &act, NULL ) != 0){maincerr << "Could not install SIGXCPU signal handler" << "\n"; exit(3);}
	//if(sigaction( SIGSEGV, &act, NULL ) != 0){maincerr << "Could not install SIGSEGV signal handler" << "\n"; exit(3);}
	if(sigaction( SIGINT, &act, NULL )  != 0){maincerr << "Could not install SIGINT signal handler" << "\n"; exit(3);}
	maincout << "Interrupt handlers installed" << "\n";
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
