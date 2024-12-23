#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_INPUT 1024
#define MAX_ARGS 100

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

void handle_redirection(char **args) {
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], ">") == 0) {
            FILE *file = fopen(args[i + 1], "w");
            if (file == NULL) {
                perror("Error opening file");
                exit(EXIT_FAILURE);
            }
            freopen(args[i + 1], "w", stdout);
            args[i] = NULL;
        }
        else if (strcmp(args[i], "<") == 0) {
            FILE *file = fopen(args[i + 1], "r");
            if (file == NULL) {
                perror("Error opening file");
                exit(EXIT_FAILURE);
            }
            freopen(args[i + 1], "r", stdin);
            args[i] = NULL;
        }
    }
}

void execute_command(char **args) {
    pid_t pid = fork();

    if (pid == 0) {
        // In child process
        execvp(args[0], args);
        perror("execvp failed");
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        perror("fork failed");
    } else {
        // In parent process
        waitpid(pid, NULL, 0);  // Wait for child process to finish
    }
}

void execute_in_background(char **args) {
    pid_t pid = fork();

    if (pid == 0) {
        // In child process
        execvp(args[0], args);
        perror("execvp failed");
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        perror("fork failed");
    } else {
        // In parent process
        background_pids[background_count++] = pid;
        printf("Background process started with PID: %d\n", pid);
    }
}

void check_background_processes() {
    for (int i = 0; i < background_count; i++) {
        pid_t pid = background_pids[i];
        int status;
        pid_t result = waitpid(pid, &status, WNOHANG);

        if (result > 0) {
            if (WIFEXITED(status)) {
                int exit_code = WEXITSTATUS(status);
                printf("[%d] retval: %d\n", pid, exit_code);
            }
            // Remove completed process from the list
            for (int j = i; j < background_count - 1; j++) {
                background_pids[j] = background_pids[j + 1];
            }
            background_count--;
            i--; // Adjust index to check the shifted process
        }
    }
}

void wait_for_background_processes() {
    for (int i = 0; i < background_count; i++) {
        pid_t pid = background_pids[i];
        int status;
        waitpid(pid, &status, 0);  // Wait for the background process to complete
        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            printf("[%d] retval: %d\n", pid, exit_code);
        }
    }
    background_count = 0;  // Reset background count after waiting
}

int main() {
    char input[MAX_INPUT];
    char *args[MAX_ARGS];

    while (1) {
        printf("> ");
        fflush(stdout);

        if (fgets(input, MAX_INPUT, stdin) == NULL) {
            break;
        }

        if (input[0] == '\n') {
            continue;
        }

        parse_input(input, args);

        if (strcmp(args[0], "quit") == 0) {
            if (background_count > 0) {
                printf("Waiting for background processes to complete...\n");
                wait_for_background_processes();
            }
            break;  // Exit the shell
        }

        int is_background = 0;
        for (int i = 0; args[i] != NULL; i++) {
            if (strcmp(args[i], "&") == 0) {
                args[i] = NULL;  // Remove the '&' from args
                is_background = 1;
                break;
            }
        }

        if (is_background) {
            execute_in_background(args);  // Start command in background
        } else {
            check_background_processes(); // Check and report background processes
            execute_command(args);  // Execute command normally
        }
    }

    return 0;
}
