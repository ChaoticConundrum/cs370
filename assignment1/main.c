#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>

const int CMD_LEN_MAX = 2048;

const int ARG_LEN_MAX = 1024;
const char *ARG_TOK = " ";
const int ARG_MAX = 100;

const int PATH_LEN_MAX = 1024;
const char *PATH = "PATH";
const char *PATH_TOK = ":";

int search(char *name){
    // Get PATH
    char *path = getenv(PATH);
    if(path == NULL){
        fprintf(stderr, "no PATH\n");
        return -1;
    }

    char program[PATH_LEN_MAX];

    // Search through PATH
    char *tok = strtok(path, PATH_TOK);
    while(tok != NULL){
        // Prepare path
        strcpy(program, tok);
        strcat(program, "/");
        strcat(program, name);
        // Check path
        int ret = access(program, X_OK);
        if(ret == 0){
            // Found program
            strcpy(name, program);
            return 0;
        }
        // Try again
        tok = strtok(NULL, PATH_TOK);
    }
    // Did not find program
    return -2;
}

int external(const char **args){

    pid_t pid = fork();

    if(pid == 0){
        // Child
        int err = execvp(args[0], args);
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

    char buffer[CMD_LEN_MAX];
    strcpy(buffer, cmd);

    int argn = 0;
    char *args[ARG_MAX];

    // Split arguments
    unsigned int len = strlen(buffer);
    char *base = buffer;
    int quote = 0;

    for(int i = 0; i < len; ++i){
        if(buffer[i] == "\n"){
            buffer[i] = 0;          // Insert terminator
            break;
        } else if(buffer[i] == '"'){
            // Hit double quote
            if(quote == 0){
                // Enter quote mode
                quote = 1;
                base = buffer + i + 1;  // Set next arg base
            } else {
                buffer[i] = 0;          // Insert terminator
                args[argn++] = base;    // Push arg
                base = buffer + i + 1;  // Set next arg base
                // Exit quote mode
                quote = 0;
            }
        } else if(quote == 0 && buffer[i] == ' '){
            // Hit space
            buffer[i] = 0;          // Insert terminator
            if(base != buffer + i)
                args[argn++] = base;    // Push arg
            base = buffer + i + 1;  // Set next arg base
        }
    }
    if(base != buffer + len - 1)
        args[argn++] = base;    // Push last arg

    printf("argn: %d\n", argn);
    for(int i = 0; i < argn; ++i){
        printf("%d: %s\n", i, args[i]);
    }

    return 0;

    /*
    char *tok = strtok(cmd, ARG_TOK);
    while(tok != NULL){

        // Prepare path
        strcpy(program, tok);
        strcat(program, "/");
        strcat(program, name);
        // Check path
        int ret = access(program, X_OK);
        if(ret == 0){
            // Found program
            strcpy(name, program);
            return 0;
        }
        // Try again
        tok = strtok(NULL, ARG_TOK);
    }

    for(int i = 0; i < CMD_LEN_MAX; ++i){
        if(cmd[i] == ' '){
            if(strlen(buffer) > 0)
                continue;
        }
    }

    // Handle command
    if(strcmp(cmd, "ping") == 0){
        printf("Pong!\n");
        return 0;
    } else {
        char path[PATH_LEN_MAX];
        strcpy(path, args[0]);
        // Search for program
        if(search(path) == 0){
            // Call external program
            external(args);
        }
    }
    */

    printf("Unknown Command\n");
    return -1;
}

int main(int argc, const char **argv){

    char cmd[CMD_LEN_MAX];

    while(1){
        // First check stdin
        if(feof(stdin)){
            // End of file
            printf("exit\n");
            break;
        }

        // Write prompt
        printf("> ");

        // Read line
        if(fgets(cmd, CMD_LEN_MAX, stdin) == NULL){
            if(feof(stdin))
                continue;
            fprintf(stderr, "fgets error\n");
            continue;
        }

        // Process input
        int ret = process(cmd);
        printf("return: %d\n", ret);
    }

    return 0;
}
