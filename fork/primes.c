#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sys/wait.h>

#define ERROR -1
#define SUCCESS 0

//Pipes positions
#define READ 0
#define WRITE 1

static void print_process_error() {
    fprintf(stderr, "PID: %i - %s\n", getpid(), strerror(errno));
}

static int get_arg_number(int argc, char* argv[]) {
    // number not specified
    if (argc != 2) return ERROR;

    int num = strtol(argv[1], NULL, 10/*base*/);

    // arg is not a number or is less than 2
    if (num < 2) return ERROR;

    return num;
}

static int gen_numbers(int wr_fd, int n) {
    for (int q = 2; q <= n; q++) {
        if (q % 2 != 0) {
            if (write(wr_fd, &q, sizeof(int)) < 0)
                return ERROR;
        }
    }
    return SUCCESS;
}

static int spawn_filter(int* fd) {
    if (pipe(fd) == ERROR)
        return ERROR;

    int pid;
    if ((pid = fork()) < 0) {
        close(fd[WRITE]);
        close(fd[READ]);
        return ERROR;
    }

    if (pid == 0)
        close(fd[WRITE]); //Child close write fd
    else
        close(fd[READ]); //Parent close read fd

    return pid;
}

static void filter(int left_rd, int n) {
    int prime; //next prime to be printed
    int ret = read(left_rd, &prime, sizeof(int));

    //parent dont find a prime
    if (ret == 0 || ret == ERROR) {
        if (ret == ERROR)
            print_process_error();
        close(left_rd);
        return;
    }

    printf("primo %i\n", prime);

    int pid, fd[2];
    if ((pid = spawn_filter(fd)) == ERROR) {
        print_process_error();
        return;
    }

    if (pid == 0) {
        close(left_rd); //is duplicated when forked
        filter(fd[READ], n);
        return;
    }

    int q;
    while (read(left_rd, &q, sizeof(int)) > 0) {
        if (q % prime != 0) {
            if (write(fd[WRITE], &q, sizeof(int)) < 0) {
                print_process_error();
                close(fd[WRITE]);
                return;
            }
        }
    }

    close(fd[WRITE]);
    close(left_rd);

    wait(NULL);
}

int main(int argc, char* argv[]) {
    int n = get_arg_number(argc, argv);

    //invalid args
    if (n == ERROR) {
        fprintf(stderr,"Numero invalido\n");
        return ERROR;
    }

    printf("primo 2\n");

    int pid, fd[2];
    if ((pid = spawn_filter(fd)) == ERROR)
        return ERROR;

    //child process
    if (pid == 0) {
        filter(fd[READ], n);
        return SUCCESS;
    }

    //continues the main process
    if (gen_numbers(fd[WRITE], n) == ERROR) {
        print_process_error();
        close(fd[WRITE]);
        return ERROR;
    }

    //close write fd when finish
    close(fd[WRITE]);

    //wait the child fork
    wait(NULL);

    return SUCCESS;
}
