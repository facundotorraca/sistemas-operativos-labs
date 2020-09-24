#include "runcmd.h"

int status = 0;
struct cmd* parsed_pipe;

// runs the command in 'cmd'
int run_cmd(char* cmd) {
    pid_t p;
	struct cmd *parsed;

	// if the "enter" key is pressed
	// just print the promt again
	if (cmd[0] == END_STRING)
	   return 0;

	// cd built-in call
	if (cd(cmd))
		return 0;

	// exit built-in call
	if (exit_shell(cmd))
		return EXIT_SHELL;

	// pwd buil-in call
	if (pwd(cmd))
		return 0;

	// parses the command line
	parsed = parse_line(cmd);

	// forks and run the command
	if ((p = fork()) == 0) {

		// keep a reference
		// to the parsed pipe cmd
		// so it can be freed later
		if (parsed->type == PIPE)
			parsed_pipe = parsed;

		exec_cmd(parsed);

        // this code is only executed
        // if exec_cmd failed to execute
        free_command(parsed);
        _exit(-1);
	}

	// store the pid of the process
	parsed->pid = p;

	// background process special treatment
    // doesn't wait for the child process
    if (parsed->type == BACK) {
        // we make that background processes have the
        // same PID as their father
        setpgid(parsed->pid, getpid());
        print_back_info(parsed);
        free_command(parsed);
        return 0;
    }

	// waits for the process to finish
    waitpid(p, &status, 0);

	print_status_info(parsed);

	free_command(parsed);

	return 0;
}
