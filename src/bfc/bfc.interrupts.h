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
	if (getrlimit(RLIMIT_AS, &t) < 0) cerr << ("could not get softlimit for RLIMIT_AS") << endl;
	t.rlim_cur = RLIM_INFINITY;
	if(setrlimit(RLIMIT_AS, &t) < 0) cerr << ("could not set softlimit for RLIMIT_AS") << endl;
}

void disable_time_limit()
{
	rlimit t;
	if (getrlimit(RLIMIT_CPU, &t) < 0) cerr << ("could not get softlimit for RLIMIT_CPU") << endl;
	t.rlim_cur = RLIM_INFINITY;
	if(setrlimit(RLIMIT_CPU, &t) < 0) cerr << ("could not set softlimit for RLIMIT_CPU") << endl;
}

void catcher(int sig)
{
	fflush(stdout);
	cerr << "Interrupted by signal ";
	switch(sig){
	case SIGXCPU:	cerr << "SIGXCPU" << endl; break;
	//case SIGSEGV:	cerr << "SIGSEGV" << endl; break; //this signal is not generated when the memory limit is reached; a malloc fails instead, causing std:bad_all to be thrown
	case SIGINT:	cerr << "SIGINT" << endl; break;
	default:		cerr << "unknown signal, terminate" << endl; _exit(7);
	}

	if(execution_state != RUNNING){
		cerr << "second interrupt; terminate" << endl;
		_exit(5);
	}else{
		cerr << "first interrupt; handle signal" << endl;
	}

	if(sig == SIGXCPU) { 
		//disable soft limit
		disable_time_limit();
		execution_state = TIMEOUT;
	}else{
		execution_state = UNKNOWN;
	}
}

void installSignalHandler()
{
	//install signal stack, otherwise SIGSEG cannot be handleded
	stack_t ss;
	const unsigned int stack_size = SIGSTKSZ*10;
	ss.ss_sp = malloc(stack_size);
	if (ss.ss_sp == NULL) { cerr << "Could not allocate signal stack context" << endl; exit(3); }
	ss.ss_size = stack_size, ss.ss_flags = 0;
	if (sigaltstack(&ss, NULL) == -1) { cerr << "Could install signal stack context" << endl; exit(3); }
	cout << "Signal stack context installed" << endl;

	//declare act to deal with action on signal set.
	static struct sigaction act;
	act.sa_handler = catcher;
	act.sa_flags = SA_ONSTACK; //act.sa_flags = 0;
	sigfillset(&(act.sa_mask));

	// install signal handler
	if(sigaction( SIGXCPU, &act, NULL ) != 0){cerr << "Could not install SIGXCPU signal handler" << endl; exit(3);}
	//if(sigaction( SIGSEGV, &act, NULL ) != 0){cerr << "Could not install SIGSEGV signal handler" << endl; exit(3);}
	if(sigaction( SIGINT, &act, NULL )  != 0){cerr << "Could not install SIGINT signal handler" << endl; exit(3);}
	cout << "Interrupt handlers installed" << endl;
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
