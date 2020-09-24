#include "utils.h"
#include "builtin.h"

extern int status;

// changes shell directory, update prompt
// adn updates status.
static void update_dir(const char* new_dir) {
    // satuts is set to 0 if the directory is changed,
    // and propmt is changed shccesfulluis;
    // non-zero otherwise.

    if (chdir(new_dir) == -1) {
        perror("change directory error");
        status = 1;
        return;
    }

    // -1 to '(' and -2 ')\0'
    char dirbuf[PRMTLEN - 3];

    if (!getcwd(dirbuf, PRMTLEN - 3)) {
        perror("update prompt error");
        status = 1;
        return;
    }

    status = 0;
    snprintf(promt, PRMTLEN, "(%s)", dirbuf);
}

// returns true if the 'exit' call
// should be performed
// (It must not be called here)
int exit_shell(char* cmd) {
    return strcmp(cmd, "exit") == 0;
}

// returns true if "chdir" was performed
//  this means that if 'cmd' contains:
// 	1. $ cd directory (change to 'directory')
// 	2. $ cd (change to HOME)
//  it has to be executed and then return true
//  Remember to update the 'prompt' with the
//  	new directory.
//
// Examples:
//  1. cmd = ['c','d', ' ', '/', 'b', 'i', 'n', '\0']
//  2. cmd = ['c','d', '\0']
int cd(char* cmd) {
	if (strlen(cmd) < 2)
        return 0;

    char* dir = NULL;

    if (strcmp(cmd, "cd") == 0)
        dir = getenv("HOME");

    if (strncmp(cmd, "cd ", 3) == 0) {
        dir = split_line(cmd, SPACE);
        dir = remove_begin_spaces(dir);
    }

    if (!dir) return 0;

    update_dir(dir);
    return 1;
}

// returns true if 'pwd' was invoked
// in the command line
// (It has to be executed here and then
// 	return true)
int pwd(char* cmd) {
    if (strcmp(cmd, "pwd") != 0)
        return 0;

    char* pwd = get_current_dir_name();

    if (!pwd) {
        perror("pwd error");
        status = 1;
    } else {
        printf("%s\n", pwd);
        status = 0;
    }

    free(pwd);
    return 1;
}
