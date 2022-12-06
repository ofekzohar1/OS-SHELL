#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>

/* MACROS */
#define ERROR_PRINT_EXIT() perror(strerror(errno)); exit(EXIT_FAILURE)
#define ERROR_PRINT_RET_ZERO() perror(strerror(errno)); return 0
#define BG_SYM "&"
#define REDIRECT_SYM ">"
#define PIPE_SYM "|"

/* Constant handlers */
const struct sigaction sigintIgnHdl = {.sa_handler = SIG_IGN, .sa_flags = SA_RESTART}; // Ignore
const struct sigaction sigintDflHdl = {.sa_handler = SIG_DFL, .sa_flags = SA_RESTART}; // Default

enum { // Enum for command type: regular/background/piping/redirect
    reg, bg, piping, redirect
} typedef COMM_TYPE;

/* Help Functions */

// Parse command type from the current arglist.
// If pipe command: save the separator index inside the given int pointer.
COMM_TYPE getCommType(int count, char **arglist, int *pipeSepIndex) {
    COMM_TYPE commType = reg;
    if (strcmp(arglist[count - 1], BG_SYM) == 0) // Lats word is &
        commType = bg;
    else if (count > 2 && strcmp(arglist[count - 2], REDIRECT_SYM) == 0)
        commType = redirect;
    else {
        for (int i = 1; i < count; i++) { // Search for pipe symbol
            if (strcmp(arglist[i], PIPE_SYM) == 0) {
                *pipeSepIndex = i; // Save separator's index
                commType = piping;
                break;
            }
        }
    }
    return commType;
}

// Run 2 piped process
int pipeProcess(char **arglist, int pipeSepIndex) {
    int pipefd[2], status;
    if (pipe(pipefd) == -1) { // pipe failed
        ERROR_PRINT_RET_ZERO();
    }

    pid_t pid1 = fork();
    if (pid1 == -1) { // fork error
        close(pipefd[0]); // Release descriptors
        close(pipefd[1]);
        ERROR_PRINT_RET_ZERO();
    } else if (!pid1) { // 1st Son
        sigaction(SIGINT, &sigintDflHdl, 0); // SIGINT to default handler
        if (close(pipefd[0]) == -1 || dup2(pipefd[1], STDOUT_FILENO) == -1 ||
            close(pipefd[1]) == -1) {
            ERROR_PRINT_EXIT();
        } // stdout to pipe write, release read side and the duplicated write descriptor

        arglist[pipeSepIndex] = NULL; // 1st command end
        execvp(arglist[0], arglist);
        ERROR_PRINT_EXIT(); // Stay on this code == execvp error
    } else { // Parent
        pid_t pid2 = fork();
        if (pid2 == -1) { // fork error
            close(pipefd[0]); // Release descriptors
            close(pipefd[1]);
            ERROR_PRINT_RET_ZERO();
        } else if (!pid2) { // 2st Son
            sigaction(SIGINT, &sigintDflHdl, 0); // SIGINT to default handler
            if (close(pipefd[1]) == -1 || dup2(pipefd[0], STDIN_FILENO) == -1 ||
                close(pipefd[0]) == -1) {
                ERROR_PRINT_EXIT();
            } // stdin to pipe read, release write side and the duplicated read descriptor

            arglist += pipeSepIndex + 1; // Second command pointer
            execvp(arglist[0], arglist);
            ERROR_PRINT_EXIT(); // Stay on this code == execvp error
        } else { // Parent continue
            close(pipefd[0]); // Release descriptors for parent
            close(pipefd[1]);
            if ((waitpid(pid1, &status, 0) == -1 || waitpid(pid2, &status, 0) == -1) &&
                errno != ECHILD && errno != EINTR) {
                ERROR_PRINT_RET_ZERO();
            }
        }
    }
    return 1; // Success
}

/* Declared in shell.c functions */

int prepare(void) {
    // ERAN'S TRICK & SIGINT to ignore for the shell process
    if (signal(SIGCHLD, SIG_IGN) == SIG_ERR || sigaction(SIGINT, &sigintIgnHdl, 0) == -1) {
        perror(strerror(errno));
        return 1;
    }
    return 0;
}

int process_arglist(int count, char **arglist) {
    int pipeSepIndex = -1; // pipe separator index
    // Get command type: regular, background, pipe, redirect
    COMM_TYPE commType = getCommType(count, arglist, &pipeSepIndex);
    pid_t pid;
    int status, fd = -1;

    if (commType == redirect) { // Open redirect file or create if non exist
        if ((fd = open(arglist[count - 1], O_RDWR | O_CREAT | O_TRUNC, S_IRWXU)) == -1) {
            ERROR_PRINT_RET_ZERO();
        }
    }
    if (commType == piping) {
        return pipeProcess(arglist, pipeSepIndex);
    } else {
        pid = fork();
        if (pid == -1) { // fork error
            ERROR_PRINT_RET_ZERO();
        } else if (!pid) { // Son
            if (commType != bg)
                sigaction(SIGINT, &sigintDflHdl, 0); // SIGINT to default handler
            else
                arglist[--count] = NULL; // Omit the & at the end of arglist

            if (commType == redirect) {
                if (dup2(fd, STDOUT_FILENO) == -1 || close(fd) == -1) {
                    ERROR_PRINT_EXIT();
                } // If dup2 was successful no need of the duplicated fd anymore
                arglist[count - 2] = NULL; // command end
            }
            execvp(arglist[0], arglist);
            ERROR_PRINT_EXIT(); // Stay on this code == execvp error
        } else { // Parent
            if (commType != bg) { // For background wait no needed
                if (waitpid(pid, &status, 0) == -1 && errno != ECHILD && errno != EINTR) {
                    ERROR_PRINT_RET_ZERO();
                }
            }
            if (commType == redirect && close(fd) == -1) { // Close redirect descriptor
                ERROR_PRINT_RET_ZERO();
            }
        }
    }

    return 1; // Success
}

int finalize(void) {
    return 0;
}