#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>    // pid_t için
#include <sys/types.h> // pid_t için ek bir garanti

#define MAX_INPUT 1024 // Maksimum komut giriş boyutu
#define MAX_ARGS 100   // Maksimum argüman sayısı

// Arka plan işlemleri
extern pid_t background_pids[MAX_ARGS]; // Arka planda çalışan işlemleri tutar
extern int background_count;           // Arka plan işlemi sayısını takip eder

void parse_input(char *input, char **args);
int handle_redirection(char **args);
void execute_command(char **args);
void execute_in_background(char **args);
void execute_pipe(char **args1, char **args2);
void check_background_processes();
void wait_for_background_processes();

#endif // SHELL_H
