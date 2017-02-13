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
        if(strlen(tok) + strlen(name) + 2 > PATH_LEN_MAX){
            fprintf(stderr, "path is too long\n");
            return -2;
        }
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
    return 1;
}

int external(const char *path, char **args){
    printf("path: %s\n", path);

    pid_t pid = fork();

    if(pid == 0){
        // Child
        int err = execvp(path, args);
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

int process(const char *input, int *status){

    char argbuff[CMD_LEN_MAX];
    strcpy(argbuff, input);

    int argn = 0;
    char *args[ARG_MAX];

    // Split arguments
    unsigned int len = strlen(argbuff);
    char *base = argbuff;
    int quote = 0;

    for(int i = 0; i < len; ++i){
        if(argbuff[i] == '\n'){
            argbuff[i] = 0;          // Insert terminator
            break;
        } else if(argbuff[i] == '"'){
            // Hit double quote
            if(quote == 0){
                // Enter quote mode
                quote = 1;
                base = argbuff + i + 1;  // Set next arg base
            } else {
                argbuff[i] = 0;          // Insert terminator
                args[argn++] = base;     // Push arg
                base = argbuff + i + 1;  // Set next arg base
                // Exit quote mode
                quote = 0;
            }
        } else if(quote == 0 && argbuff[i] == ' '){
            // Hit space
            argbuff[i] = 0;          // Insert terminator
            if(base != argbuff + i)
                args[argn++] = base; // Push arg
            base = argbuff + i + 1;  // Set next arg base
        }
    }
    if(base != argbuff + len - 1)
        args[argn++] = base;    // Push last arg

    args[argn] = 0; // terminate arg list
    *status = 0;
    const char *cmd = args[0];

//    printf("input '%s'\n", input);
//    printf("argn %d\n", argn);
//    for(int i = 0; i < argn; ++i)
//        printf("arg %d: '%s'\n", i, args[i]);

    // Handle command
    if(strcmp(cmd, "exit") == 0){
        // Exit shell
        *status = 1;

    } else if(strcmp(cmd, "echo") == 0){
        // Echo arguments
        char buffer[CMD_LEN_MAX];
        buffer[0] = 0;
        for(int i = 1; i < argn; ++i){
            strcat(buffer, args[i]);
            strcat(buffer, " ");
        }
        printf("%s\n", buffer);

    } else if(strcmp(cmd, "cd") == 0){
        if(argn > 1){
            // Change directory
            chdir(args[1]);
        }

    } else if(strcmp(cmd, "pwd") == 0){
        // Print working directory
        char buffer[PATH_LEN_MAX];
        getcwd(buffer, sizeof(buffer));
        printf("%s\n", buffer);

    } else {
        // External program
        char path[PATH_LEN_MAX];
        strcpy(path, cmd);
        // Search for program
        int ret = search(path);
        if(ret == 0){
            *status = 3;
            // Call external program
            return external(path, args);
        } else if(ret < 0){
            // error
            *status = 4;
        }
        // Program not found
        *status = 2;
    }
    return 0;
}

int main(int argc, const char **argv){

    char cmd[CMD_LEN_MAX];
    int status = 0;

    while(1){
        // First check stdin
        if(feof(stdin)){
            // End of file
            printf("exit\n");
            break;
        }

        // Write prompt
        printf("choong-sh> ");

        // Read line
        if(fgets(cmd, CMD_LEN_MAX, stdin) == NULL){
            if(feof(stdin))
                continue;
            fprintf(stderr, "fgets error\n");
            continue;
        }

        // Process input
        int ret = process(cmd, &status);
//        printf("status %d\n", status);

        if(status == 0){
            // internal command
        } else if(status == 1){
            // exit
            break;
        } else if(status == 2){
            // unknown command
            printf("Unknown Command\n");
        } else if(status == 3){
            // external command
            printf("return: %d\n", ret);
        }
    }

    return 0;
}
