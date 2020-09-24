#include "parsing.h"

extern int status;

// parses an argument of the command stream input
static char* get_token(char* buf, int idx) {

	char* tok;
	int i;

	tok = (char*)calloc(ARGSIZE, sizeof(char));
	i = 0;

	while (buf[idx] != SPACE && buf[idx] != END_STRING) {
		tok[i] = buf[idx];
		i++; idx++;
	}

	return tok;
}

// returns the file to redirect error output.
// returns null in case of out_file not specified
static char* get_err_redir(char* redir, char* out_file) {
    if (strcmp(redir, "&1") == 0) {
        if (strlen(out_file) != 0)
            return out_file;
        return NULL; // redir stderr to stdout
    }
    return redir;
}

// redirect STDOUT to rdfile.
// appout flag is set to true or false, depending on rdfile first char
// (> means to append)
static void redir_stdout(struct execcmd* c, char* rdfile) {
    if (strlen(rdfile) == 0)
        return;

    c->appout = false;
    char* outfile = rdfile;

    if (rdfile[0] == '>' && strlen(rdfile) > 1) {
        outfile = &rdfile[1];
        c->appout = true;
    }

    strcpy(c->out_file, outfile);
}

// redirect STDERR to rdfile. If rdfile is '&1', STDERR is redirected
// to out_file. If outfile is not specified, STDERR is not redirected.
// apperr flag is set to true or flase, depending on rdfile first char
// (> means to append)
static void redir_stderr(struct execcmd* c, char* rdfile) {
    if (strlen(rdfile) == 0)
        return;

    c->apperr = false;
    char* errfile = get_err_redir(rdfile, c->out_file);

    // stodout not specied, keep error output
    // in stderr
    if (!errfile)
        return;

    if (rdfile[0] == '>' && strlen(rdfile) > 1) {
        //2>>&1 do not exist as a commando so we use &1 as a file
        errfile = &rdfile[1];
        c->apperr = true;
    }

    strcpy(c->err_file, errfile);
}

// parses and changes stdin/out/err if needed
static bool parse_redir_flow(struct execcmd* c, char* arg) {

	int inIdx, outIdx;

	// flow redirection for output
	if ((outIdx = block_contains(arg, '>')) >= 0) {
		switch (outIdx) {
			case 0: {
                redir_stdout(c, &arg[1]);
				break;
			}

			case 1: {
                if (arg[0] == '2')
                    redir_stderr(c, &arg[2]);

                if (arg[0] == '&') {
                    redir_stdout(c, &arg[2]);
                    redir_stderr(c, &arg[2]);
                }

                break;
			}
		}

		free(arg);
		c->type = REDIR;

		return true;
	}

	// flow redirection for input
	if ((inIdx = block_contains(arg, '<')) >= 0) {
		// stdin redir
		strcpy(c->in_file, arg + 1);

		c->type = REDIR;
		free(arg);

		return true;
	}

	return false;
}

// parses and sets a pair KEY=VALUE
// environment variable
static bool parse_environ_var(struct execcmd* c, char* arg) {

	// sets environment variables apart from the
	// ones defined in the global variable "environ"
	if (block_contains(arg, '=') > 0) {

		// checks if the KEY part of the pair
		// does not contain a '-' char which means
		// that it is not a environ var, but also
		// an argument of the program to be executed
		// (For example:
		// 	./prog -arg=value
		// 	./prog --arg=value
		// )
		if (block_contains(arg, '-') < 0) {
			c->eargv[c->eargc++] = arg;
			return true;
		}
	}

	return false;
}

// this function will be called for every token, and it should
// expand environment variables. In other words, if the token
// happens to start with '$', the correct substitution with the
// environment value should be performed. Otherwise the same
// token is returned.
static char* expand_environ_var(char* arg) {
	if (arg[0] != '$') // not environ var
        return arg;

    char* val;

    if (strcmp(arg, "$?") == 0) {
        char statbuf[BUFLEN];
        snprintf(statbuf, BUFLEN, "%i", status);
        val = statbuf;
    } else
        val = getenv(&arg[1]);

    // if there is no match
    // return empty string
    if (!val) {
        arg[0] = '\0';
        return arg;
    }

    size_t len_arg = strlen(arg);
    size_t len_val = strlen(val);

    if (len_val > len_arg)
        arg = realloc(arg, (len_val + 1/* \0 */) * sizeof(char));

    strcpy(arg, val);
    return arg;
}

// parses one single command having into account:
// - the arguments passed to the program
// - stdin/stdout/stderr flow changes
// - environment variables (expand and set)
static struct cmd* parse_exec(char* buf_cmd) {

	struct execcmd* c;
	char* tok;
	int idx = 0, argc = 0;

	c = (struct execcmd*)exec_cmd_create(buf_cmd);

	while (buf_cmd[idx] != END_STRING) {

		tok = get_token(buf_cmd, idx);
		idx = idx + strlen(tok);

		if (buf_cmd[idx] != END_STRING)
			idx++;

		if (parse_redir_flow(c, tok))
			continue;

		if (parse_environ_var(c, tok))
			continue;

		tok = expand_environ_var(tok);

		c->argv[argc++] = tok;
	}

	c->argv[argc] = (char*)NULL;
	c->argc = argc;

	return (struct cmd*)c;
}

// parses a command knowing that it contains
// the '&' char
static struct cmd* parse_back(char* buf_cmd) {

	int i = 0;
	struct cmd* e;

	while (buf_cmd[i] != '&')
		i++;

	buf_cmd[i] = END_STRING;

	e = parse_exec(buf_cmd);

	return back_cmd_create(e);
}

// parses a command and checks if it contains
// the '&' (background process) character
static struct cmd* parse_cmd(char* buf_cmd) {

	if (strlen(buf_cmd) == 0)
		return NULL;

	int idx;

	// checks if the background symbol is after
	// a redir symbol, in which case
	// it does not have to run in in the 'back'
	if ((idx = block_contains(buf_cmd, '&')) >= 0 &&
		 buf_cmd[idx - 1] != '>' &&
         buf_cmd[idx + 1] != '>')
		return parse_back(buf_cmd);

	return parse_exec(buf_cmd);
}

// parses the command line
// looking for the pipe character '|'
struct cmd* parse_line(char* buf) {
	struct cmd *r, *l;

	char* right = split_line(buf, '|');

	l = parse_cmd(buf);
    r = NULL;

    // if there is a right command
    // it can be also a PIPE
    //
    // ex:
    //                [c1]
    //               /
    // [c1 | c2 | c3]           [c2]
    //               \         /
    //                [c2 | c3]
    //                         \.
    //                          [c3]

    if (strlen(right) > 0)
        r = parse_line(right);

	return pipe_cmd_create(l, r);
}
