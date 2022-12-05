#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

//
#define ERROR_PRINT_EXIT() perror(strerror(errno)); exit(EXIT_FAILURE)
#define BG_SYM "&"
#define REDIRECT_SYM ">"
#define PIPE_SYM ">"

enum { // Regular command is 0, background 1, piping 2, redirect 3
    reg, bg, piping, redirect
} typedef COMM_TYPE;

COMM_TYPE getCommType(int count, char **arglist, int *pipeSepIndex) {
    COMM_TYPE commType = reg;
    if (strcmp(arglist[count - 1], BG_SYM) == 0) // Lats word is &
        commType = bg;
    else if (count > 3 && strcmp(arglist[count - 2], REDIRECT_SYM) == 0)
        commType = redirect;
    else {
        for (int i = 1; i < count; i++) {
            if (strcmp(arglist[i], PIPE_SYM) == 0) {
                *pipeSepIndex = i;
                commType = piping;
            }
        }
    }
    return commType;
}

void sigint_handler() {

}

int pipe_sep_locator(int count, char **arglist) {
    // for (int i = 1; i < count)
}

//

int prepare(void) {
    // ERAN'S TRICK
    if (signal(SIGCHLD, SIG_IGN) == SIG_ERR) {
        perror(strerror(errno));
        return 1;
    }
    struct sigaction handler = {.sa_handler = SIG_IGN, .sa_flags = SA_RESTART};
    sigaction(SIGINT, &handler, 0);
    return 0;
}

int process_arglist(int count, char **arglist) {
    int pipeSepIndex = -1; // pipe seperator index
    // Get command type: regular, background, pipe, redirect
    COMM_TYPE commType = getCommType(count, arglist, &pipeSepIndex);
    pid_t pid;
    int status;

    switch (commType) {
        case reg:
            pid = fork();
            if (pid == -1) { // fork error
                ERROR_PRINT_EXIT();
            } else if(!pid) { // Son
                if (execvp(arglist[0], arglist) == -1) { // execvp error
                    ERROR_PRINT_EXIT();
                }
            } else { // Parent
                if (waitpid(pid, &status, 0) == -1 && errno != ECHILD) {
                    ERROR_PRINT_EXIT();
                }
            }
            break;
        case redirect:
        case bg:
        case piping:
        default:
            break;
    }
    return 1;
}

int finalize(void) {
    return 0;
}