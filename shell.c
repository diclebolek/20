#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include "shell.h"

pid_t background_pids[MAX_ARGS];
int background_count = 0;

void parse_input(char *input, char **args) {
    char *token = strtok(input, " \t\n");
    int i = 0;
    while (token != NULL) {
        args[i++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[i] = NULL;
}

int handle_redirection(char **args) {
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], ">") == 0) {
            int fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                perror("Çıkış dosyası açılamadı");
                return -1;
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
            args[i] = NULL;
        } else if (strcmp(args[i], "<") == 0) {
            int fd = open(args[i + 1], O_RDONLY);
            if (fd < 0) {
                fprintf(stderr, "Giriş dosyası bulunamadı: %s\n", args[i + 1]);
                return -1;
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
            args[i] = NULL;
        }
    }
    return 0;
}

void execute_command(char **args) {
    pid_t pid = fork();
    if (pid == 0) {
        if (handle_redirection(args) == -1) {
            exit(EXIT_FAILURE);
        }
        execvp(args[0], args);
        perror("execvp failed");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        waitpid(pid, NULL, 0);
    } else {
        perror("fork failed");
    }
}

void execute_in_background(char **args) {
    pid_t pid = fork();
    if (pid == 0) {
        if (handle_redirection(args) == -1) {
            exit(EXIT_FAILURE);
        }
        execvp(args[0], args);
        perror("execvp failed");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        background_pids[background_count++] = pid;
        printf("Arka planda çalışan işlem başlatıldı, PID: %d\n", pid);
    } else {
        perror("fork failed");
    }
}

void execute_pipe(char **args1, char **args2) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe failed");
        return;
    }
    pid_t pid1 = fork();
    if (pid1 == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        execvp(args1[0], args1);
        perror("execvp failed");
        exit(EXIT_FAILURE);
    }
    pid_t pid2 = fork();
    if (pid2 == 0) {
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[1]);
        close(pipefd[0]);
        execvp(args2[0], args2);
        perror("execvp failed");
        exit(EXIT_FAILURE);
    }
    close(pipefd[0]);
    close(pipefd[1]);
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
}

void check_background_processes() {
    for (int i = 0; i < background_count; i++) {
        int status;
        pid_t result = waitpid(background_pids[i], &status, WNOHANG);
        if (result > 0) {
            if (WIFEXITED(status)) {
                printf("[%d] retval: %d\n", background_pids[i], WEXITSTATUS(status));
            }
            for (int j = i; j < background_count - 1; j++) {
                background_pids[j] = background_pids[j + 1];
            }
            background_count--;
            i--;
        }
    }
}

void wait_for_background_processes() {
    for (int i = 0; i < background_count; i++) {
        int status;
        waitpid(background_pids[i], &status, 0);
        if (WIFEXITED(status)) {
            printf("[%d] retval: %d\n", background_pids[i], WEXITSTATUS(status));
        }
    }
    background_count = 0;
}
