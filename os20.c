#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_INPUT 1024
#define MAX_ARGS 100

pid_t background_pids[MAX_ARGS];
int background_count = 0;

// Function to split input into arguments
void parse_input(char *input, char **args) {
    char *token = strtok(input, " \t\n");
    int i = 0;

    while (token != NULL) {
        args[i++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[i] = NULL;
}

// Function to handle I/O redirection
void handle_redirection(char **args) {
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], ">") == 0) {
            int fd = open(args[i + 1], O_CREAT | O_WRONLY | O_TRUNC, 0644);
            if (fd < 0) {
                perror("Error opening file");
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
            args[i] = NULL;
        } else if (strcmp(args[i], "<") == 0) {
            int fd = open(args[i + 1], O_RDONLY);
            if (fd < 0) {
                perror("Error opening file");
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
            args[i] = NULL;
        }
    }
}

// Function to execute commands
void execute_command(char **args) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("Fork failed");
    } else if (pid == 0) {
        handle_redirection(args);
        if (execvp(args[0], args) < 0) {
            perror("Command execution failed");
            exit(EXIT_FAILURE);
        }
    } else {
        wait(NULL);
    }
}

// Function to handle background processes
void execute_in_background(char **args) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("Fork failed");
    } else if (pid == 0) {
        handle_redirection(args);
        if (execvp(args[0], args) < 0) {
            perror("Command execution failed");
            exit(EXIT_FAILURE);
        }
    } else {
        background_pids[background_count++] = pid;
        printf("Background process started with PID: %d\n", pid);
    }
}

// Function to wait for all background processes to complete
void wait_for_background_processes() {
    for (int i = 0; i < background_count; i++) {
        int status;
        waitpid(background_pids[i], &status, 0);
        printf("Background process with PID %d completed\n", background_pids[i]);
    }
    background_count = 0;
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

        // Check for empty input
        if (input[0] == '\n') {
            continue;
        }

        // Parse input into arguments
        parse_input(input, args);

        // Quit command
        if (strcmp(args[0], "quit") == 0) {
            if (background_count > 0) {
                printf("Waiting for background processes to complete...\n");
                wait_for_background_processes();
            }
            break;
        }

        // Check for background execution
        int is_background = 0;
        for (int i = 0; args[i] != NULL; i++) {
            if (strcmp(args[i], "&") == 0) {
                args[i] = NULL;
                is_background = 1;
                break;
            }
        }

        if (is_background) {
            execute_in_background(args);
        } else {
            execute_command(args);
        }
    }

    return 0;
}
