/*
 * Basit bir Linux kabuk (shell) uygulaması
 * Bu program, temel komut satırı işlemlerini gerçekleştirebilen bir shell uygulamasıdır.
 * Özellikleri:
 * - Temel komutları çalıştırma
 * - Pipe (|) kullanarak komutları zincirleme
 * - Input/Output yönlendirme (< ve >)
 * - Arka planda çalıştırma (&)
 * - Dizin değiştirme (cd komutu)
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

/* Sabit tanımlamalar */
#define MAX_COMMAND_LENGTH 1024    // Maksimum komut uzunluğu
#define MAX_ARGUMENTS 100          // Bir komut için maksimum argüman sayısı
#define MAX_BG_PROCESSES 100       // Maksimum arka plan işlem sayısı

/* Global değişkenler */
pid_t background_processes[MAX_BG_PROCESSES];  // Arka plan işlemlerinin PID'lerini tutan dizi
int bg_process_count = 0;                      // Mevcut arka plan işlem sayısı

/* Komut istemini ekrana yazdıran fonksiyon */
void print_prompt()
{
    // Komut istemini ekrana yazdır
    printf("> ");
    fflush(stdout);
}

/*
 * Tek bir komutu çalıştıran fonksiyon
 * Parametreler:
 * - command: Çalıştırılacak komut dizisi
 * İşlevler:
 * - Komutu argümanlara ayırır
 * - Input/Output yönlendirmelerini işler
 * - Arka plan çalıştırma kontrolü yapar
 * - Özel komutları (cd, quit) işler
 */
void execute_command(char *command)
{
    // Komut argümanlarını ve yönlendirme değişkenlerini tanımla
    char *args[MAX_ARGUMENTS];
    int arg_count = 0;
    char *input_file = NULL, *output_file = NULL;
    int background = 0;

    // Komutu boşluklara göre parçala ve argümanları işle
    char *token = strtok(command, " ");
    while (token != NULL && arg_count < MAX_ARGUMENTS - 1)
    {
        // Input yönlendirmesi (<) kontrolü
        if (strcmp(token, "<") == 0)
        {
            token = strtok(NULL, " ");
            if (token)
                input_file = token;
        }
            // Output yönlendirmesi (>) kontrolü
        else if (strcmp(token, ">") == 0)
        {
            token = strtok(NULL, " ");
            if (token)
                output_file = token;
        }
            // Arka plan işlemi (&) kontrolü
        else if (strcmp(token, "&") == 0)
        {
            background = 1;
        }
            // Normal argüman ise diziye ekle
        else
        {
            args[arg_count++] = token;
        }
        token = strtok(NULL, " ");
    }
    args[arg_count] = NULL;

    // Boş komut kontrolü
    if (arg_count == 0)
        return;

    // Çıkış komutu kontrolü
    if (strcmp(args[0], "quit") == 0)
    {
        printf("Exiting shell...\n");
        exit(0);
    }

    // cd (dizin değiştirme) komutu kontrolü
    if (strcmp(args[0], "cd") == 0)
    {
        if (arg_count < 2)
        {
            // Argüman verilmemişse ev dizinine git
            char *home = getenv("HOME");
            if (home != NULL && chdir(home) != 0)
            {
                perror("cd");
            }
        }
        else if (chdir(args[1]) != 0)
        {
            perror("cd");
        }
        return;
    }

    // Yeni bir süreç oluştur
    pid_t pid = fork();
    if (pid < 0)
    {
        perror("Fork failed");
        return;
    }

    if (pid == 0)
    {
        // Çocuk süreç işlemleri

        // Input yönlendirmesi varsa dosyayı aç ve stdin'e yönlendir
        if (input_file)
        {
            int fd_in = open(input_file, O_RDONLY);
            if (fd_in < 0)
            {
                perror("Input file error");
                exit(EXIT_FAILURE);
            }
            dup2(fd_in, STDIN_FILENO);
            close(fd_in);
        }

        // Output yönlendirmesi varsa dosyayı aç ve stdout'a yönlendir
        if (output_file)
        {
            int fd_out = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd_out < 0)
            {
                perror("Output file error");
                exit(EXIT_FAILURE);
            }
            dup2(fd_out, STDOUT_FILENO);
            close(fd_out);
        }

        // Komutu çalıştır
        if (execvp(args[0], args) < 0)
        {
            perror("Command execution failed");
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        // Ana süreç işlemleri
        if (background)
        {
            // Arka plan işlemi ise PID'i kaydet ve devam et
            printf("[PID %d] Running in background\n", pid);
            if (bg_process_count < MAX_BG_PROCESSES)
            {
                background_processes[bg_process_count++] = pid;
            }
        }
        else
        {
            // Ön plan işlemi ise çocuk sürecin bitmesini bekle
            int status;
            waitpid(pid, &status, 0);
        }
    }
}

/*
 * Kullanıcı girdisini işleyen ve uygun şekilde çalıştıran fonksiyon
 * Parametreler:
 * - input: Kullanıcıdan alınan komut satırı
 * İşlevler:
 * - Pipe içeren ve içermeyen komutları ayırt eder
 * - Uygun çalıştırma fonksiyonunu çağırır
 */
void parse_and_execute(char *input)
{
    char *commands[MAX_ARGUMENTS];
    int command_count = 0;

    // Split input by ';'
    char *token = strtok(input, ";");
    while (token != NULL && command_count < MAX_ARGUMENTS - 1)
    {
        commands[command_count++] = token;
        if (strstr(token, "<") != NULL)
        {
            // This will fix error showing after prompt > {Error}
            char sleep[] = "sleep 0.2";
            commands[command_count++] = sleep;
        }
        token = strtok(NULL, ";");
    }
    commands[command_count] = NULL;

    for (int i = 0; i < command_count; i++)
    {
        if (strchr(commands[i], '|') != NULL)
        {
            // TODO - Pipe işlemini gerçekleştir
//            execute_piped_commands(commands[i]);
        }
        else
        {
            execute_command(commands[i]);
        }
    }
}

/*
 * Ana fonksiyon
 * İşlevler:
 * - SIGCHLD sinyalini yakalar
 * - Sonsuz döngüde kullanıcı komutlarını bekler
 * - Komutları işlemek için gerekli fonksiyonları çağırır
 */
int main()
{

    char input[MAX_COMMAND_LENGTH];

    // Ana program döngüsü
    while (1)
    {
        // Komut istemini göster
        print_prompt();

        if (fgets(input, MAX_COMMAND_LENGTH, stdin) == NULL)
        {
            perror("Input reading failed");
            continue;
        }

    }

    return 0;
}
