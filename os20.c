#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_INPUT 1024 // Maksimum komut giriş boyutu
#define MAX_ARGS 100   // Maksimum argüman sayısı

pid_t background_pids[MAX_ARGS]; // Arka planda çalışan işlemleri tutar
int background_count = 0;        // Arka plan işlemi sayısını takip eder

// Komut girişini parçalar ve argüman dizisine atar
void parse_input(char *input, char **args) {
    char *token = strtok(input, " \t\n"); // Boşluk ve yeni satır karakterlerinden ayrıştır
    int i = 0;

    while (token != NULL) {
        args[i++] = token; // Argümanı kaydet
        token = strtok(NULL, " \t\n");
    }
    args[i] = NULL; // Argüman listesinin sonuna NULL ekle
}

// Giriş ve çıkış yönlendirme operatörlerini işler
int handle_redirection(char **args) {
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], ">") == 0) { // Çıkışı bir dosyaya yönlendir
            int fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                perror("Çıkış dosyası açılamadı");
                return -1; // Hata durumunda -1 döndür
            }
            dup2(fd, STDOUT_FILENO); // Standart çıktıyı dosyaya yönlendir
            close(fd);
            args[i] = NULL; // Yönlendirme operatörünü argüman listesinden kaldır
        } else if (strcmp(args[i], "<") == 0) { // Girişi bir dosyadan okuma
            int fd = open(args[i + 1], O_RDONLY);
            if (fd < 0) {
                fprintf(stderr, "Giriş dosyası bulunamadı: %s\n", args[i + 1]);
                return -1;
            }
            dup2(fd, STDIN_FILENO); // Standart girdiyi dosyaya yönlendir
            close(fd);
            args[i] = NULL; // Yönlendirme operatörünü argüman listesinden kaldır
        }
    }
    return 0; // Başarıyla tamamlandı
}

// Tekli komutları çalıştırır
void execute_command(char **args) {
    pid_t pid = fork(); // Yeni bir işlem oluştur

    if (pid == 0) { // Çocuk işlem
        if (handle_redirection(args) == -1) { // Yönlendirme hatası kontrolü
            exit(EXIT_FAILURE);
        }
        execvp(args[0], args); // Komutu çalıştır
        perror("execvp failed");
        exit(EXIT_FAILURE); // Komut çalıştırılamazsa çık
    } else if (pid > 0) { // Ebeveyn işlem
        waitpid(pid, NULL, 0); // Çocuk işlemin bitmesini bekle
    } else {
        perror("fork failed");
    }
}

// Arka planda çalışan komutları işler
void execute_in_background(char **args) {
    pid_t pid = fork();

    if (pid == 0) { // Çocuk işlem
        if (handle_redirection(args) == -1) { // Yönlendirme hatası kontrolü
            exit(EXIT_FAILURE);
        }
        execvp(args[0], args); // Komutu çalıştır
        perror("execvp failed");
        exit(EXIT_FAILURE);
    } else if (pid > 0) { // Ebeveyn işlem
        background_pids[background_count++] = pid; // Arka plan işlemini kaydet
        printf("Arka planda çalışan işlem başlatıldı, PID: %d\n", pid);
    } else {
        perror("fork failed");
    }
}

// Borulama komutlarını işler
void execute_pipe(char **args1, char **args2) {
    int pipefd[2]; // Pipe için dosya tanımlayıcıları
    if (pipe(pipefd) == -1) {
        perror("pipe failed");
        return;
    }

    pid_t pid1 = fork(); // İlk işlem
    if (pid1 == 0) {
        dup2(pipefd[1], STDOUT_FILENO); // İlk işlemin çıktısını pipe'a yönlendir
        close(pipefd[0]);
        close(pipefd[1]);
        execvp(args1[0], args1);
        perror("execvp failed");
        exit(EXIT_FAILURE);
    }

    pid_t pid2 = fork(); // İkinci işlem
    if (pid2 == 0) {
        dup2(pipefd[0], STDIN_FILENO); // İkinci işlemin girdisini pipe'dan al
        close(pipefd[1]);
        close(pipefd[0]);
        execvp(args2[0], args2);
        perror("execvp failed");
        exit(EXIT_FAILURE);
    }

    close(pipefd[0]); // Pipe kapatılır
    close(pipefd[1]);
    waitpid(pid1, NULL, 0); // İlk işlemi bekle
    waitpid(pid2, NULL, 0); // İkinci işlemi bekle
}

// Arka plan işlemlerini kontrol eder
void check_background_processes() {
    for (int i = 0; i < background_count; i++) {
        int status;
        pid_t result = waitpid(background_pids[i], &status, WNOHANG);

        if (result > 0) { // İşlem tamamlandı
            if (WIFEXITED(status)) {
                printf("[%d] retval: %d\n", background_pids[i], WEXITSTATUS(status));
            }
            for (int j = i; j < background_count - 1; j++) {
                background_pids[j] = background_pids[j + 1];
            }
            background_count--;
            i--; // Liste güncellendiği için indeksi yeniden ayarla
        }
    }
}

// Tüm arka plan işlemlerinin tamamlanmasını bekler
void wait_for_background_processes() {
    for (int i = 0; i < background_count; i++) {
        int status;
        waitpid(background_pids[i], &status, 0);
        if (WIFEXITED(status)) {
            printf("[%d] retval: %d\n", background_pids[i], WEXITSTATUS(status));
        }
    }
    background_count = 0; // Sayacı sıfırla
}

// Ana işlev
int main() {
    char input[MAX_INPUT];
    char *args[MAX_ARGS];
    char *pipe_args1[MAX_ARGS];
    char *pipe_args2[MAX_ARGS];

    while (1) {
        printf("> "); // Komut istemi
        fflush(stdout);

        if (fgets(input, MAX_INPUT, stdin) == NULL) { // Kullanıcı girişini oku
            break; // EOF veya hata durumunda çık
        }

        if (input[0] == '\n') { // Boş satırları atla
            continue;
        }

        parse_input(input, args); // Komutu ayrıştır

        if (strcmp(args[0], "quit") == 0) { // Çıkış komutu
            if (background_count > 0) { // Arka plan işlemleri varsa
                printf("Arka plandaki işlemler tamamlanıyor...\n");
                wait_for_background_processes();
            }
            break;
        }

        int is_pipe = 0, pipe_index = -1;
        for (int i = 0; args[i] != NULL; i++) { // Borulama kontrolü
            if (strcmp(args[i], "|") == 0) {
                is_pipe = 1;
                pipe_index = i;
                args[i] = NULL;
                break;
            }
        }

        if (is_pipe) { // Borulama işlemi varsa
            memcpy(pipe_args1, args, sizeof(char *) * pipe_index);
            memcpy(pipe_args2, &args[pipe_index + 1], sizeof(char *) * (MAX_ARGS - pipe_index));
            execute_pipe(pipe_args1, pipe_args2);
        } else {
            int is_background = 0;
            for (int i = 0; args[i] != NULL; i++) { // Arka planda çalışma kontrolü
                if (strcmp(args[i], "&") == 0) {
                    args[i] = NULL;
                    is_background = 1;
                    break;
                }
            }

            if (is_background) {
                execute_in_background(args);
            } else {
                check_background_processes(); // Arka plan işlemleri kontrolü
                execute_command(args); // Tekli komutları çalıştır
            }
        }
    }

    return 0;
}
