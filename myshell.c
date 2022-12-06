#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>

//
#define ERROR_PRINT_EXIT() perror(strerror(errno)); exit(EXIT_FAILURE)
#define ERROR_PRINT_RET_ZERO() perror(strerror(errno)); return 0
#define BG_SYM "&"
#define REDIRECT_SYM ">"
#define PIPE_SYM ">"

const struct sigaction sigintIgnHdl = {.sa_handler = SIG_IGN, .sa_flags = SA_RESTART};
const struct sigaction sigintDflHdl = {.sa_handler = SIG_DFL, .sa_flags = SA_RESTART};

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

int pipeProcess(int count, char **arglist, int pipeSepIndex) {
    int pipefd[2], status;
    if (pipe(pipefd) == -1) { // pipe failed
        perror(strerror(errno));
        return 0;
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

        arglist[pipeSepIndex] = NULL;
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
            } // stdin to pipe read, release wrtie side and the duplicated read descriptor

            arglist[pipeSepIndex] = arglist[0];
            execvp(arglist[0], arglist + pipeSepIndex);
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

//

int prepare(void) {
    // ERAN'S TRICK
    if (signal(SIGCHLD, SIG_IGN) == SIG_ERR || sigaction(SIGINT, &sigintIgnHdl, 0) == -1) {
        perror(strerror(errno));
        return 1;
    }
    return 0;
}

int process_arglist(int count, char **arglist) {
    int pipeSepIndex = -1; // pipe seperator index
    // Get command type: regular, background, pipe, redirect
    COMM_TYPE commType = getCommType(count, arglist, &pipeSepIndex);
    pid_t pid;
    int status;

    if (commType == piping){
        return pipeProcess(count, arglist, pipeSepIndex);
    } else {
        pid = fork();
        if (pid == -1) { // fork error
            ERROR_PRINT_RET_ZERO();
        } else if(!pid) { // Son
            if (commType != bg)
                sigaction(SIGINT, &sigintDflHdl, 0); // SIGINT to default handler
            else
                arglist[--count] = NULL; // Omit the & at the end of arglist

            if (commType == redirect) {
                int fd = open(arglist[count - 1], O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);
                if (fd == -1 || dup2(fd, STDOUT_FILENO) == -1 || close(fd) == -1) {
                    ERROR_PRINT_EXIT();
                }
                // If dup2 was successful no need of the duplicated fd anymore
            }
            execvp(arglist[0], arglist);
            ERROR_PRINT_EXIT(); // Stay on this code == execvp error
        } else { // Parent
            if(commType != bg) { // For background wait no needed
                if (waitpid(pid, &status, 0) == -1 && errno != ECHILD && errno != EINTR) {
                    ERROR_PRINT_RET_ZERO();
                }
            }
        }
    }

    return 1;
}

int finalize(void) {
    return 0;
}