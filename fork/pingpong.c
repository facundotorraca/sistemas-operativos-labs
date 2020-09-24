#define _XOPEN_SOURCE 500 //random

#include <time.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

#define ERROR -1
#define SUCCESS 0

//P ---> parent process
//C ---> child process

#define READ 0
#define WRITE 1

static int init_pipes(int* pc_fd, int* cp_fd) {
    if (pipe(pc_fd) == ERROR || pipe(cp_fd) == ERROR)
        return ERROR;

    printf("  - pipe me devuelve: [%i, %i]\n", pc_fd[READ], pc_fd[WRITE]);
    printf("  - pipe me devuelve: [%i, %i]\n", cp_fd[READ], cp_fd[WRITE]);
    return SUCCESS;
}

static void do_child_process(int* pc_fd, int* cp_fd, int pid) {
    close(pc_fd[WRITE]); //Close write P--->C fd
    close(cp_fd[READ]); //Close read C--->P fd

    int rand_recv;
    assert(read(pc_fd[READ], &rand_recv, sizeof(int)) > 0);

    printf("\nDonde fork me devuelve %i:\n", pid);
    printf("  - getpid me devuelve: %i\n", getpid());
    printf("  - getppid me devuelve: %i\n", getppid());
    printf("  - recibo valor %i vía fd=%i\n", rand_recv, pc_fd[WRITE]);
    printf("  - reenvío valor en fd=%i y termino\n", cp_fd[READ]);

    assert(write(cp_fd[WRITE], &rand_recv, sizeof(int)) > 0);

    //Close fd's after comunication
    close(pc_fd[READ]);
    close(cp_fd[WRITE]);
}

static void do_parent_process(int* pc_fd, int* cp_fd, int pid) {
    close(pc_fd[READ]); //Close read P--->C fd
    close(cp_fd[WRITE]); //Close write C--->P fd

    //set a new seed to random using current time
    srandom(time(NULL));
    int randval = random();

    printf("\nDonde fork me devuelve %i:\n", pid);
    printf("  - getpid me devuelve: %i\n", getpid());
    printf("  - getppid me devuelve: %i\n", getppid());
    printf("  - random me devuelve: %i\n", randval);
    printf("  - envío valor %i a través de fd=%i\n", randval, pc_fd[WRITE]);

    assert(write(pc_fd[WRITE], &randval, sizeof(int)) > 0);

    int rand_recv;
    assert(read(cp_fd[READ], &rand_recv, sizeof(int)) > 0);

    printf("\nHola, de nuevo PID %i:\n", getpid());
    printf("  - recibí valor %i vía fd=%i\n", rand_recv, cp_fd[READ]);

    //Close fd's after comunication
    close(pc_fd[WRITE]);
    close(cp_fd[READ]);
}

int main(void) {
    printf("Hola, soy PID %i:\n",  getpid());

    int pc_fd[2];  //parent->child pipe
    int cp_fd[2];  //child->parent pipe

    if (init_pipes(pc_fd, cp_fd) == ERROR) {
        perror("Pipe initi error");
        return ERROR;
    }

    int pid;
    if ((pid = fork()) < 0) {
        perror("Fork error");
        return ERROR;
    }

    if (pid == 0)
        do_child_process(pc_fd, cp_fd, pid);
    else
        do_parent_process(pc_fd, cp_fd, pid);

    return SUCCESS;
}
