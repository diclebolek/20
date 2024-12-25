#include <stdio.h>
#include <string.h>
#include "shell.h"

int main() {
    char input[MAX_INPUT];
    char *args[MAX_ARGS];
    char *pipe_args1[MAX_ARGS];
    char *pipe_args2[MAX_ARGS];

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
                printf("Arka plandaki işlemler tamamlanıyor...\n");
                wait_for_background_processes();
            }
            break;
        }

        int is_pipe = 0, pipe_index = -1;
        for (int i = 0; args[i] != NULL; i++) {
            if (strcmp(args[i], "|") == 0) {
                is_pipe = 1;
                pipe_index = i;
                args[i] = NULL;
                break;
            }
        }

        if (is_pipe) {
            memcpy(pipe_args1, args, sizeof(char *) * pipe_index);
            memcpy(pipe_args2, &args[pipe_index + 1], sizeof(char *) * (MAX_ARGS - pipe_index));
            execute_pipe(pipe_args1, pipe_args2);
        } else {
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
                check_background_processes();
                execute_command(args);
            }
        }
    }

    return 0;
}
