#include "exec.h"
#include <errno.h>
#include <string.h>

// sets the "key" argument with the key part of
// the "arg" argument and null-terminates it
static void get_environ_key(char* arg, char* key) {

	int i;
	for (i = 0; arg[i] != '='; i++)
		key[i] = arg[i];

	key[i] = END_STRING;
}

// sets the "value" argument with the value part of
// the "arg" argument and null-terminates it
static void get_environ_value(char* arg, char* value, int idx) {

	size_t i, j;
	for (i = (idx + 1), j = 0; i < strlen(arg); i++, j++)
		value[j] = arg[i];

	value[j] = END_STRING;
}

// sets the environment variables received
// in the command line
static void set_environ_vars(char** eargv, int eargc) {
    char key[ARGSIZE];
    char val[ARGSIZE];

    int idx;

    for (int i = 0; i < eargc; i++) {
        idx = block_contains(eargv[i], '=');

        get_environ_key(eargv[i], key);
        get_environ_value(eargv[i], val, idx);

        // 1 enable overwriting the envirion variable
        if (setenv(key, val, 1/*overwrite*/) == -1)
            perror("environment variable error");
    }
}

// executes the command specified in (e)
// only returns in case of error
static void exec_execcmd(struct execcmd* e) {
    // program args
    char* const* args = e->argv;

    set_environ_vars(e->eargv, e->eargc);

    // execvp failed to run command
    if (execvp(args[0], args) == -1)
        perror("execvp error");
}

// opens the file in which the stdin/stdout or
// stderr flow will be redirected and redirect to the
// newfd and returns the file descriptor.
static int open_redir_fd(char* file, int flags, int newfd) {
    int mode = 0;

    // O_CREAT: if pathname does not exist,
    // create it as a regular file.
    if ((flags & (O_CREAT | O_WRONLY)) != 0)
        // S_IRUSR: user has permissions to read
        // S_IWUSR: user has permissions to write
        mode = S_IWUSR | S_IRUSR;

    int oldfd = open(file, flags | O_CLOEXEC, mode);

    if (oldfd < 0) {
        fprintf(stderr, "cant open file %s: %s\n", file, strerror(errno));
        return -1;
    }

    // creates a copy of the file descriptor oldfd in the file de‐
    // scriptor number specified in newfd. If the file descriptor newfd
    // was previously open, it is silently closed before being reused
    dup2(oldfd, newfd);

	return oldfd;
}

// redirect input, output and/or error output
// according to the file specified in (r)
// only returns in case of error
static void exec_rdircmd(struct execcmd* r) {
    // To check if a redirection has to be performed
    // verify if file name's length (in the execcmd struct)
    // is greater than zero

    int in_fd = -1;
    int out_fd = -1;
    int err_fd = -1;

    if (strlen(r->in_file) > 0) {
        // makes stdin(0) points to in_file
        in_fd = open_redir_fd(r->in_file, O_RDONLY, STDIN_FILENO);

        if (in_fd < 0) // cant open input file
            return;
    }

    if (strlen(r->out_file) > 0) {
        int outflags =  O_CREAT | O_WRONLY;

        if (r->appout) outflags |= O_APPEND;
        else outflags |= O_TRUNC;

        // makes stdout(1) points to out_file
        out_fd = open_redir_fd(r->out_file, outflags, STDOUT_FILENO);

        if (out_fd < 0) { // cant open or create output file
            if (in_fd != -1) close(in_fd);
            return;
        }
    }

    if (strlen(r->err_file) > 0) {
        int errflags =  O_CREAT | O_WRONLY;

        if (r->apperr) errflags |= O_APPEND;
        else errflags |= O_TRUNC;

        // makes stderr(2) points to err_file
        err_fd = open_redir_fd(r->err_file, errflags, STDERR_FILENO);

        if (err_fd < 0) { // cant open or create output error file
//            if (in_fd != -1) close(in_fd);
//            if (out_fd != -1) close(out_fd);
            return;
        }
    }

    r->type = EXEC;
    exec_cmd((struct cmd*)r);

    // this code is only executed
    // if execvp failed to execute
    if (in_fd != -1) close(in_fd);
    if (out_fd != -1) close(out_fd);
    if (err_fd != -1) close(err_fd);
}

// fork and execute left pipe command.
// modifies pidl with the pid of the fork which executes
// left command.
// in case of error, pidl is filled with -1 and closes the
// FD's pointed bt fd_lr
static void exec_lpipe(struct cmd* lcmd, int* fd_lr, int* pidl) {
    if (((*pidl) = fork()) == -1) {
        perror("fork error");
        close(fd_lr[WRITE]);
        close(fd_lr[READ]);
        return;
    }

    if ((*pidl) == 0) {
        // parent process
        close(fd_lr[READ]);
        // redirect STDOUT --> pipe write end
        dup2(fd_lr[WRITE], STDOUT_FILENO);

        close(fd_lr[WRITE]);
        exec_cmd(lcmd);

        // this code is only executed
        // if execvp failed to execute
        free_command(parsed_pipe);
        close(fd_lr[WRITE]);
        _exit(-1);
    }
}

// fork and execute right pipe command.
// modifies pidr with the pid of the fork which executes
// right command.
// in case of error, pidr is filled with -1 and closes the
// FD's pointed bt fd_lr
static void exec_rpipe(struct cmd* rcmd, int* fd_lr, int* pidr) {
    if (((*pidr) = fork()) == -1) {
        perror("fork error");
        close(fd_lr[WRITE]);
        close(fd_lr[READ]);
        return;
    }

    if ((*pidr) == 0) {
        // child process
        close(fd_lr[WRITE]);
        // redirect STDIN --> pipe read end
        dup2(fd_lr[READ], STDIN_FILENO);

        close(fd_lr[READ]);
        exec_cmd(rcmd);

        // this code is only executed
        // if execvp failed to execute
        free_command(parsed_pipe);
        close(fd_lr[READ]);
        exit(-1);
    }
}

// create a pipe between two commands
// specified in (p).
// returns right command status
static void exec_pipecmd(struct pipecmd* p) {
    int fd_lr[2]; // L --> R
    if (pipe(fd_lr) == -1) {
        perror("pipe error");
        return;
    }

    pid_t pidl = -1;
    exec_lpipe(p->leftcmd, fd_lr, &pidl);

    pid_t pidr = -1;
    exec_rpipe(p->rightcmd, fd_lr, &pidr);


    close(fd_lr[READ]);
    close(fd_lr[WRITE]);

    int rstatus = -1;

    if (pidl != -1) waitpid(pidl, NULL, 0);
    if (pidr != -1) waitpid(pidr, &rstatus, 0);

    // never return to runcmd
    // so memory is free here
    free_command(parsed_pipe);

    _exit(unmask(rstatus));
}

// executes a command
// only returns in case of error
void exec_cmd(struct cmd* cmd) {

	switch (cmd->type) {

		case EXEC: {
            // spawns a command
            struct execcmd* e;
            e = (struct execcmd*)cmd;
			exec_execcmd(e);

            // returns to caller. If previous
            // command was BACK or PIPE, previous
            // call to exec_cmd returns to run_cmd
            // and free the correct allocated memory
            return;
        }

		case BACK: {
			// runs a command in background
            struct backcmd* b;
			b = (struct backcmd*)cmd;
            exec_cmd(b->c);
            return;
		}

		case REDIR: {
			// changes the input/output/stderr flow
            struct execcmd* r;
            r = (struct execcmd*)cmd;
            exec_rdircmd(r);
            return;
		}

		case PIPE: {
			// pipes two commands
            struct pipecmd* p;
			p = (struct pipecmd*)cmd;
            exec_pipecmd(p);
		}
	}
}
