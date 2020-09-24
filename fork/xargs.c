#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>

#define ERROR -1
#define SUCCESS 0
#define DEFAULT_CMD "echo"

#ifndef NARGS
#define NARGS 4
#endif

/*----------------allocation-functions-------------*/
static char** _read_cmd(int argc, char* argv[], char* def_cmd) {
    //command count without xargs call
    int cmdc = argc - 1;

    if (cmdc == 0) { //No command specified
        char** cmds = calloc(1, sizeof(char*));
        cmds[0] = def_cmd;
        return cmds;
    }

    char** cmds = calloc(cmdc, sizeof(char*));

    //Load each command from argv
    for (int i = 1; i < argc; i++)
        cmds[i-1] = argv[i];

    return cmds;
}

static char** _init_args(char* cmds[], int cmdc) {
    //+NARGS for argument +1 for null termination
    char** args = calloc(cmdc + NARGS + 1, sizeof(char*));

    //copy commands and flags in args buff
    for (int i = 0; i < cmdc; i++)
        args[i] = cmds[i];

    //null termination for execvp args
    args[cmdc + NARGS] = NULL;

    return args;
}

static void _free_args(char* const args[], int cmdc, int total_args) {
    for (int i = 0; i < total_args; i++)
        free(args[cmdc + i]);
}
/*-------------------------------------------------*/

static int read_arg(char** arg_buff) {
    char* aux_buff = NULL; size_t n = 0;

    //allocates mem for the entire line
    ssize_t chars_rd = getline(&aux_buff, &n, stdin);

    if (chars_rd == ERROR || chars_rd == 0) {
        free(aux_buff);
        return ERROR;
    }

    //copy line ptr on args buffer
    (*arg_buff) = aux_buff;

    (*arg_buff)[chars_rd-1] = '\0'; //remove endline char
    return SUCCESS;
}

static int execute_command(char* const args[]) {
    int pid = fork();
    if (pid == 0) {
        //execvp free all allocated args
        if (execvp(args[0], args) == ERROR) {
            perror("execvp error");
            return pid;
        }
    } else {
        wait(NULL); //wait program ends
    }

    return pid;
}

int main(int argc, char* argv[]) {
    //used in case no command specified
    char* def_cmd = DEFAULT_CMD;

    //number of commands (not counting xargs call)
    //if no cmds specified, cmdc = 1 (DEFAULT_CMD)
    int cmdc = ((argc - 1) > 0) ? argc - 1 : 1;

    char** cmds = _read_cmd(argc, argv, def_cmd);
    char** args = _init_args(cmds, cmdc);

    size_t args_read = 0;
    while (read_arg(&args[cmdc + args_read]) == SUCCESS) {
        args_read++;

        if (args_read == NARGS) {

            int pid = execute_command(args);

            //in case of error, child fork
            //returs and free all memory
            if (pid  == 0) {
                _free_args(args, cmdc, NARGS);
                free(args);
                free(cmds);
                exit(ERROR);
            }

            _free_args(args, cmdc, NARGS);
            args_read = 0;
        }
    }

    //last execution with less args
    if (args_read != 0) {
        args[cmdc + args_read] = NULL;

        int pid = execute_command(args);

        //in case of error, child fork
        //returs and free all memory
        if (pid == 0) {
            _free_args(args, cmdc, args_read);
            free(args);
            free(cmds);
            exit(ERROR);
        }

        //only free the args read
        _free_args(args, cmdc, args_read);
    }

    free(cmds);
    free(args);

    return SUCCESS;
}
