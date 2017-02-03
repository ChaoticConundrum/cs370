#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>

#define CMD_LEN_MAX 1024
#define ARG_MAX     10
#define ARG_LEN_MAX 1024

int external(const char *name, const char **args){

    pid_t pid = fork();

    if(pid == 0){
        // Child
        int err = execl();
        // we shouldn't be here
        fprintf(stderr, "execl error %d %d %s\n", err, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if(pid == -1){
        // Fork error, in parent
        fprintf(stderr, "fork error %d %s\n", errno, strerror(errno));
        return -1;
    }

    // Parent
    int wstatus;
    pid_t wpid = waitpid(pid, &wstatus, 0);
    if(wpid != pid){
        fprintf(stderr, "waitpid error %d\n", wpid);
    }

    return 0;
}

int process(const char *cmd){

    char args[ARG_MAX][ARG_LEN_MAX];

    char buffer[ARG_LEN_MAX];
    buffer[0] = 0;

    for(int i = 0; i < CMD_LEN_MAX; ++i){
        if(cmd[i] == ' '){
            if(strlen(buffer) > 0)
                continue;
        }
    }

    if(strcmp(cmd, "ping") == 0){
        printf("Pong!\n");
    } else {
        printf("Unknown Command\n");
        return -1;
    }

    return 0;
}

int main(int argc, const char **argv){

    char cmd[CMD_LEN_MAX];

    while(1){

        printf("> ");

        if(fgets(cmd, CMD_LEN_MAX, stdin) == NULL){
            fprintf(stderr, "fgets error\n");
            continue;
        }

        int ret = process(cmd);
        printf("return: %d\n", ret);

    }

    return 0;
}
