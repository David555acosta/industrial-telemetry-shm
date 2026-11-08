#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <signal.h>
#include <time.h>

#include "common.h"

static volatile sig_atomic_t corriendo = 1;

static void cortar(int s) {
    (void)s;
    corriendo = 0;
}

int main(void) {
    // Captura de SIGINT (Ctrl+C) para apagado controlado
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = cortar;
    sigaction(SIGINT, &sa, NULL);

    printf("[SENSOR] Iniciando proceso emisor de telemetría...\n");

    // 1. Crear / Abrir el segmento de memoria compartida POSIX
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Error al ejecutar shm_open");
        exit(EXIT_FAILURE);
    }

    // 2. Establecer el tamaño exacto del struct
    if (ftruncate(shm_fd, sizeof(TelemetriaCompresor)) == -1) {
        perror("Error al configurar el tamaño con ftruncate");
        close(shm_fd);
        exit(EXIT_FAILURE);
    }

    // 3. Mapear la memoria compartida en la tabla de páginas del proceso
    TelemetriaCompresor* telemetria = mmap(NULL, sizeof(TelemetriaCompresor),
        PROT_READ | PROT_WRITE, MAP_SHARED,
        shm_fd, 0);
    if (telemetria == MAP_FAILED) {
        perror("Error al mapear la memoria con mmap");
        close(shm_fd);
        exit(EXIT_FAILURE);
    }

    // 4. Inicializar semáforo en memoria compartida (pshared = 1, valor inicial = 1)
    if (sem_init(&telemetria->mutex, 1, 1) == -1) {
        perror("Error al inicializar semáforo con sem_init");
        munmap(telemetria, sizeof(TelemetriaCompresor));
        close(shm_fd);
        exit(EXIT_FAILURE);
    }

    printf("Sensor iniciado. Memoria mapeada en la dirección virtual %p\n", (void*)telemetria);
    printf("------------------------------------------------------------------------------------------------\n");
    printf(" SECUENCIA | FECHA Y HORA         | TEMP (°C) | PRES (Bar) | VIB (µm) | RPM   | ESTADO\n");
    printf("------------------------------------------------------------------------------------------------\n");
    printf("Ctrl+C para terminar.\n");

    uint64_t secuencia = 0;

    while (corriendo) {
        time_t t_actual = time(NULL);
        struct tm tiempo_info;
        localtime_r(&t_actual, &tiempo_info);

        double base = (double)(secuencia % 100);

        // --- INICIO SECCIÓN CRÍTICA ---
        LOCK(telemetria);

        telemetria->numero_secuencia = ++secuencia;
        telemetria->timestamp = t_actual;

        telemetria->anio = tiempo_info.tm_year + 1900;
        telemetria->mes = tiempo_info.tm_mon + 1;
        telemetria->dia = tiempo_info.tm_mday;
        telemetria->hora = tiempo_info.tm_hour;
        telemetria->minuto = tiempo_info.tm_min;
        telemetria->segundo = tiempo_info.tm_sec;

        telemetria->temperatura_cojinetes = (float)base;
        telemetria->presion_descarga = (float)(base * 2.0);
        telemetria->vibracion_axial = (float)(base * 3.0);
        telemetria->velocidad_rpm = 9500.0f;
        telemetria->estado_alerta = 0;

        // Formateo y salida impresos dentro de la exclusión mutua
        char fecha_hora_str[32];
        snprintf(fecha_hora_str, sizeof(fecha_hora_str), "%04d-%02d-%02d %02d:%02d:%02d",
            telemetria->anio, telemetria->mes, telemetria->dia,
            telemetria->hora, telemetria->minuto, telemetria->segundo);

        printf(" %9lu | %s | %9.1f | %10.1f | %8.1f | %5.0f | %s\n",
            telemetria->numero_secuencia,
            fecha_hora_str,
            telemetria->temperatura_cojinetes,
            telemetria->presion_descarga,
            telemetria->vibracion_axial,
            telemetria->velocidad_rpm,
            telemetria->estado_alerta == 0 ? "NORMAL" : "ALERTA");

        UNLOCK(telemetria);
        // --- FIN SECCIÓN CRÍTICA ---

        if (secuencia % 10000 == 0) {
            printf("Medición %lu completada\n", secuencia);
        }

        usleep(100);
    }

    printf("\n[SENSOR] Apagando y liberando recursos del Kernel...\n");

    // Limpieza de recursos
    sem_destroy(&telemetria->mutex);
    munmap(telemetria, sizeof(TelemetriaCompresor));
    close(shm_fd);
    shm_unlink(SHM_NAME);

    printf("[SENSOR] Proceso finalizado correctamente. Mediciones escritas: %lu\n", secuencia);

    return 0;
}