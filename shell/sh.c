#include "defs.h"
#include "types.h"
#include "runcmd.h"
#include <signal.h>
#include "readline.h"

char promt[PRMTLEN] = {0};

// sig      The number of the signal that caused invocation of the handler.
//
// info     a structure  containing  further information about the signal
//
// ucontext This  is  a  pointer  to a ucontext_t structure, cast to void *.
//          The structure pointed to by this field contains  signal  context
//          information  that  was saved on the user-space stack by the ker‐
//          nel;
static void backproc_handler(int sig, siginfo_t* info, void* ucontext) {
    // if group id is equal to current process id, then the process is
    // a background process
    if (getpgid(info->si_pid) == getpid()) {
        pid_t bgpid = waitpid(0, NULL, 0);
        printf("==> terminado : PID= %i \n", bgpid);
    }
}

// set the handler to deal with background processes asynchronously
static void set_backproc_handler() {
    struct sigaction sigact;
    // If SA_SIGINFO  is specified in sa_flags, then sa_sigaction (instead of
    // sa_handler) specifies the signal-handling function  for  signum.
    //
    // SA_RESTART is specified in sa_flags to making certain system calls
    // restartable across signals, so the program can continue running after
    // handling the signal
    sigact.sa_flags = SA_SIGINFO | SA_RESTART;

    // sa_sigaction specifies the action to be associated with signum.
    // This function receives the signal number as its only argument.
    sigact.sa_sigaction = backproc_handler;

    // Makes the main process to handler the SIGCHILD signal with
    // the handler specified in 'sigaction'.
    // Previous action is not relevant to be saved,
    // then NULL is passed as a parameter.
    sigaction(SIGCHLD, &sigact, NULL);
}

// runs a shell command
static void run_shell() {

	char* cmd;

	while ((cmd = read_line(promt)) != NULL)
		if (run_cmd(cmd) == EXIT_SHELL)
			return;
}

// initialize the shell with the "HOME" directory
static void init_shell() {

	char buf[BUFLEN] = {0};
	char* home = getenv("HOME");

	if (chdir(home) < 0) {
		snprintf(buf, sizeof buf, "cannot cd to %s ", home);
		perror(buf);
	} else {
		snprintf(promt, sizeof promt, "(%s)", home);
	}

    set_backproc_handler();
}

int main(void) {

	init_shell();

	run_shell();

	return 0;
}
