#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <omp.h>  // <- OpenMP debe ir aquí arriba

#define N 9

int grilla[N][N]; // Arreglo global

bool columnasValidas = true;
bool filasValidas = true;

// Función del thread para validar columnas
void* revisarColumnas(void* arg) {
    omp_set_num_threads(9);      // Ideal: 9 columnas
    omp_set_nested(true);        // Permitir anidación

    pid_t tid = syscall(SYS_gettid);
    printf("- El thread que ejecuta el método para revisión de columnas es: %d\n", tid);

    #pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < N; i++) {
        pid_t tid_col = syscall(SYS_gettid);
        printf("- En la revisión de columnas el siguiente es un thread en ejecución: %d\n", tid_col);

        bool presente[N + 1] = { false };
        for (int j = 0; j < N; j++) {
            int num = grilla[j][i];
            if (num < 1 || num > 9 || presente[num]) {
                columnasValidas = false;
                break;
            }
            presente[num] = true;
        }
    }

    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    omp_set_num_threads(1); // Paso 1: ejecutarlo todo en serie inicialmente

    if (argc < 2) {
        fprintf(stderr, "Uso: %s <archivo_sudoku>\n", argv[0]);
        return 1;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("No se pudo abrir el archivo");
        return 1;
    }

    char *contenido = mmap(NULL, 81, PROT_READ, MAP_PRIVATE, fd, 0);
    if (contenido == MAP_FAILED) {
        perror("mmap falló");
        close(fd);
        return 1;
    }

    // Cargar grilla
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            char c = contenido[i * N + j];
            grilla[i][j] = c - '0';
        }
    }

    munmap(contenido, 81);
    close(fd);

    pid_t pid_padre = getpid();
    pid_t pid1 = fork();

    if (pid1 == 0) {
        char pid_str[16];
        snprintf(pid_str, sizeof(pid_str), "%d", pid_padre);
        execlp("ps", "ps", "-p", pid_str, "-lLf", NULL);
        perror("execlp falló");
        exit(1);
    }

    pthread_t hilo_columna;
    pthread_create(&hilo_columna, NULL, revisarColumnas, NULL);
    pthread_join(hilo_columna, NULL);

    printf("- El thread en el que se ejecuta main es: %ld\n", syscall(SYS_gettid));
    waitpid(pid1, NULL, 0);

    // Revisión de filas
    omp_set_num_threads(9);
    omp_set_nested(true);
    #pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < N; i++) {
        bool presente[N + 1] = { false };
        for (int j = 0; j < N; j++) {
            int num = grilla[i][j];
            if (num < 1 || num > 9 || presente[num]) {
                filasValidas = false;
                break;
            }
            presente[num] = true;
        }
    }

    // Revisión de subcuadros 3x3 desde (0,0), (3,3), (6,6)
    bool subcuadrosValidos = true;
    omp_set_num_threads(3);
    omp_set_nested(true);
    #pragma omp parallel for schedule(dynamic)
    for (int k = 0; k < 3; k++) {
        int i = k * 3;
        bool presente[N + 1] = { false };
        for (int x = i; x < i + 3; x++) {
            for (int y = i; y < i + 3; y++) {
                int num = grilla[x][y];
                if (num < 1 || num > 9 || presente[num]) {
                    subcuadrosValidos = false;
                    break;
                }
                presente[num] = true;
            }
        }
    }

    if (filasValidas && columnasValidas && subcuadrosValidos)
        printf("¡Sudoku válido!\n");
    else
        printf("Sudoku inválido.\n");

    pid_t pid2 = fork();
    if (pid2 == 0) {
        char pid_str[16];
        snprintf(pid_str, sizeof(pid_str), "%d", pid_padre);
        execlp("ps", "ps", "-p", pid_str, "-lLf", NULL);
        perror("execlp falló");
        exit(1);
    }

    waitpid(pid2, NULL, 0);
    return 0;
}
