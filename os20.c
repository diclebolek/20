#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_INPUT 1024 
#define MAX_ARGS 100   

pid_t background_pids[MAX_ARGS];  // Arka planda çalışan işlem PID'lerini tutar
int background_count = 0;         // Arka plan işlemi sayısını takip eder

// Kullanıcıdan alınan komutu parçalara böler
void parse_input(char *input, char **args) {
    char *token = strtok(input, " \t\n");  // Komutu boşluk ve yeni satırdan böl
    int i = 0;

    while (token != NULL) {
        args[i++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[i] = NULL;  // Argümanlar dizisinin sonuna NULL ekle
}

// Yönlendirme operatörlerini ("<" ve ">") kontrol eder ve uygun dosyaları açar
void handle_redirection(char **args) {
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], ">") == 0) {  // Çıkışı bir dosyaya yönlendirme
            freopen(args[i + 1], "w", stdout);
            args[i] = NULL;  // Yönlendirme operatörünü kaldır
        }
        else if (strcmp(args[i], "<") == 0) {  // Girişi bir dosyadan okuma
            freopen(args[i + 1], "r", stdin);
            args[i] = NULL;
        }
    }
}

// Verilen komutu çalıştırır (normal modda)
void execute_command(char **args) {
    pid_t pid = fork();  // Yeni bir işlem oluştur

    if (pid == 0) {
        // Çocuk işlem
        execvp(args[0], args);  // Komutu çalıştır
        perror("execvp failed");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        // Ebeveyn işlem
        waitpid(pid, NULL, 0);  // Çocuk işlemin bitmesini bekle
    } else {
        perror("fork failed");  // fork başarısız oldu
    }
}

// Verilen komutu arka planda çalıştırır
void execute_in_background(char **args) {
    pid_t pid = fork();

    if (pid == 0) {
        execvp(args[0], args);  // Çocuk işlemde komutu çalıştır
        perror("execvp failed");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        // Ebeveyn işlem
        background_pids[background_count++] = pid;  // PID'yi kaydet
        printf("Arka planda çalışan işlem başlatıldı, PID: %d\n", pid);
    } else {
        perror("fork failed");
    }
}

// Arka plandaki işlemleri kontrol eder ve tamamlananları raporlar
void check_background_processes() {
    for (int i = 0; i < background_count; i++) {
        int status;
        pid_t result = waitpid(background_pids[i], &status, WNOHANG);

        if (result > 0) {  // İşlem tamamlandı
            if (WIFEXITED(status)) {
                printf("[%d] retval: %d\n", background_pids[i], WEXITSTATUS(status));
            }
            // Tamamlanan işlemi listeden çıkar
            for (int j = i; j < background_count - 1; j++) {
                background_pids[j] = background_pids[j + 1];
            }
            background_count--;
            i--;  // Liste güncellendiği için indeksi ayarla
        }
    }
}

// Tüm arka plan işlemlerinin bitmesini bekler
void wait_for_background_processes() {
    for (int i = 0; i < background_count; i++) {
        int status;
        waitpid(background_pids[i], &status, 0);
        if (WIFEXITED(status)) {
            printf("[%d] retval: %d\n", background_pids[i], WEXITSTATUS(status));
        }
    }
    background_count = 0;  // Sayacı sıfırla
}

int main() {
    char input[MAX_INPUT];
    char *args[MAX_ARGS];

    while (1) {
        printf("> ");  // Komut istemi
        fflush(stdout);

        if (fgets(input, MAX_INPUT, stdin) == NULL) {
            break;  // EOF veya hata durumunda döngüyü bitir
        }

        if (input[0] == '\n') {
            continue;  // Boş satır için bir şey yapma
        }

        parse_input(input, args);

        if (strcmp(args[0], "quit") == 0) {  // Çıkış komutu
            if (background_count > 0) {
                printf("Arka plandaki işlemler tamamlanıyor...\n");
                wait_for_background_processes();
            }
            break;
        }

        int is_background = 0;
        for (int i = 0; args[i] != NULL; i++) {
            if (strcmp(args[i], "&") == 0) {  // Arka planda çalıştırma kontrolü
                args[i] = NULL;  // '&' işaretini kaldır
                is_background = 1;
                break;
            }
        }

        if (is_background) {
            execute_in_background(args);
        } else {
            check_background_processes();  // Önce arka plandaki işlemleri kontrol et
            execute_command(args);  // Komutu normal şekilde çalıştır
        }
    }

    return 0;
}
