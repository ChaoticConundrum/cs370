#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>

#define PROMPT "choong-sh>"

const int CMD_LEN_MAX = 2048;

const int ARG_LEN_MAX = 1024;
const char *ARG_TOK = " ";
const int ARG_MAX = 100;

const int PATH_LEN_MAX = 1024;
const char *PATH = "PATH";
const char *PATH_TOK = ":";
const char *HOME = "HOME";

const int TIME_LEN_MAX = 64;

static int debug = 1;

static int histlen = 0;
static int histsize = 0;
static char **history = NULL;

void timeval_str(struct timeval tv, char *buff){
    time_t nowtime;
    struct tm *nowtm;
    char tmbuf[TIME_LEN_MAX];

    nowtime = tv.tv_sec;
    nowtm = localtime(&nowtime);
    strftime(tmbuf, sizeof(tmbuf), "%Y-%m-%d %H:%M:%S", nowtm);
    snprintf(buff, TIME_LEN_MAX, "%s.%06d", tmbuf, tv.tv_usec);
}

int external(const char *path, char **args){
    // Fork process
    pid_t pid = fork();

    if(pid == 0){
        // Child, execute program
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

    // capture exit status
    int ret = 0;
    if(WIFEXITED(wstatus)){
        ret = WEXITSTATUS(wstatus);
        struct rusage usage;
        if(getrusage(RUSAGE_CHILDREN, &usage) != 0){
            fprintf(stderr, "getrusage error %d %s\n", errno, strerror(errno));
            return ret;
        }

        if(debug){
            printf("** STATS for %s **\n", path);

            printf("user CPU time used: %ld.%06d sec\n", usage.ru_utime.tv_sec, usage.ru_utime.tv_usec);
            printf("system CPU time used: %ld.%06d sec\n", usage.ru_stime.tv_sec, usage.ru_stime.tv_usec);

            printf("maximum resident set size: %ld kB\n",        usage.ru_maxrss);
//            printf("integral shared memory size: %ld\n",      usage.ru_ixrss);
//            printf("integral unshared data size: %ld\n",      usage.ru_idrss);
//            printf("integral unshared stack size: %ld\n",     usage.ru_isrss);
            printf("page reclaims (soft page faults): %ld\n", usage.ru_minflt);
            printf("page faults (hard page faults): %ld\n",   usage.ru_majflt);
//            printf("swaps: %ld\n",                            usage.ru_nswap);
            printf("block input operations: %ld\n",           usage.ru_inblock);
            printf("block output operations: %ld\n",          usage.ru_oublock);
//            printf("IPC messages sent: %ld\n",                usage.ru_msgsnd);
//            printf("IPC messages received: %ld\n",            usage.ru_msgrcv);
//            printf("signals received: %ld\n",                 usage.ru_nsignals);
            printf("voluntary context switches: %ld\n",       usage.ru_nvcsw);
            printf("involuntary context switches: %ld\n",     usage.ru_nivcsw);
        }
    }

    return ret;
}

int search(char *name){
    if(strlen(name) == 0)
        return 2;

    // Try path verbatim
    if(name[0] == '/' || name[0] == '.'){
        int ret = access(name, X_OK);
        if(ret == 0){
            return 0;
        }
    }

    // Get PATH
    const char *path = getenv(PATH);
    if(path == NULL){
        fprintf(stderr, "no PATH\n");
        return -1;
    }
    char pathbuff[PATH_LEN_MAX];
    strcpy(pathbuff, path);

    // Search through PATH
    char program[PATH_LEN_MAX];
    char *tok = strtok(pathbuff, PATH_TOK);

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
        // Try next path
        tok = strtok(NULL, PATH_TOK);
    }

    // Did not find program
    return 1;
}

void add_history(const char *cmd){
    if(histlen + 1 > histsize){
        histsize = histsize + 100;
        // New buffer
        char **tmp = malloc(histsize * sizeof(const char *));
        // Copy
        for(int i = 0; i < histlen; ++i){
            tmp[i] = history[i];
        }
        free(history);
        history = tmp;
    }

    // Store command
    char *stor = malloc(strlen(cmd));
    strcpy(stor, cmd);
    history[histlen++] = stor;
}

int process(const char *input, int *status){
    *status = 0;
    if(strlen(input) == 0) return 0;

    char argbuff[CMD_LEN_MAX];
    strcpy(argbuff, input);

    int argn = 0;
    char *args[ARG_MAX];

    // Split arguments
    unsigned int len = strlen(argbuff);
    char *base = argbuff;
    int quote = 0;

    for(int i = 0; i < len; ++i){
        if(argbuff[i] == '"'){
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
    if(base != argbuff + len)
        args[argn++] = base;    // Push last arg

    args[argn] = 0; // terminate arg list
    const char *cmd = args[0];

    // debug
//    printf("input '%s'\n", input);
//    printf("argn %d\n", argn);
//    for(int i = 0; i < argn; ++i)
//        printf("arg %d: '%s'\n", i, args[i]);

    int savehist = 1;

    // Handle command
    int ret = 0;
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
            if(strcmp(args[1], "~") == 0){
                // Home directory
                const char *home = getenv(HOME);
                if(home == NULL){
                    fprintf(stderr, "no HOME\n");
                    return -1;
                }
                printf("%s\n", home);
                chdir(home);

            } else {
                chdir(args[1]);
            }
        }

    } else if(strcmp(cmd, "pwd") == 0){
        // Print working directory
        char buffer[PATH_LEN_MAX];
        getcwd(buffer, sizeof(buffer));
        printf("%s\n", buffer);

    } else if(strcmp(cmd, "nodebug") == 0){
        debug = 0;

    } else if(strcmp(cmd, "history") == 0){
        savehist = 0;
        for(int i = 0; i < histlen; ++i){
            printf("%d\t%s\n", i, history[i]);
        }

    } else if(strcmp(cmd, "!!") == 0){
        savehist = 0;
        if(histlen > 0){
            const char *prev = history[histlen - 1];
            printf("%s\n", prev);
            return process(prev, status);
        } else {
            printf("no history!\n");
        }

    } else if(cmd[0] == '!'){
        savehist = 0;
        int num = atoi(cmd+1);
        if(histlen > num){
            const char *prev = history[num];
            printf("%s\n", prev);
            return process(prev, status);
        } else {
            printf("not in history!\n");
        }

    } else if(strcmp(cmd, "help") == 0){
        printf("exit - exit the shell\n");
        printf("echo - echo arguments\n");
        printf("cd - change working directory\n");
        printf("pwd - print working directory\n");
        printf("history - print history\n");
        printf("!! - run last command in history\n");
        printf("!n - run nth command in history\n");

    } else {
        // External program
        char path[PATH_LEN_MAX];
        strcpy(path, cmd);
        // Search for program
        int stat = search(path);
        if(stat < 0){
            // error
            *status = 4;
        } else if(stat > 0){
            // Program not found
            *status = 2;
        } else {
            // Call external program
            *status = 3;
            ret = external(path, args);
        }
    }

    if(savehist){
        // save command to history
        add_history(input);
    }

    return ret;
}

void sig_handler(int sig){
//    printf("sigint caught\n");
//    fprintf(stdin, "\n");
}

int main(int argc, const char **argv){
    int status = 0;
    char cmd[CMD_LEN_MAX];

//    signal(SIGINT, sig_handler);

    while(1){
        // First check stdin
        if(feof(stdin)){
            // End of file
            printf("exit\n");
            break;
        }

        // Write prompt
        printf(PROMPT " ");

        // Read line
        if(fgets(cmd, CMD_LEN_MAX, stdin) == NULL){
            if(feof(stdin))
                continue;
            fprintf(stderr, "fgets error\n");
            continue;
        }

        // first newline becomes a terminator
        for(int i = 0; i < strlen(cmd); ++i){
            if(cmd[i] == '\n'){
                cmd[i] = 0;
                break;
            }
        }

        // Process input
        int ret = process(cmd, &status);
//        printf("status %d\n", status);

        if(status == 0){
            // internal command
            continue;
        } else if(status == 1){
            // exit
            break;
        } else if(status == 2){
            // unknown command
            printf("Unknown Command\n");
        } else if(status == 3){
            // external command
            if(debug)
                printf("return: %d\n", ret);
        }
    }

    for(int i = 0; i < histlen; ++i){
        free(history[i]);
    }
    free(history);

    return 0;
}
