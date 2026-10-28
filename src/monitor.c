#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <time.h>

#include "common.h"



int main(void) {
   
    printf("[MONITOR] Conectándose al sistema de telemetría del compresor...\n");


    int shm_fd = shm_open(SHM_NAME, O_RDONLY, 0666);
    if (shm_fd == -1) {
        perror("Error al abrir shm_open (¿Está el sensor encendido?)");
        exit(EXIT_FAILURE);
    }

    TelemetriaCompresor* telemetria = mmap(NULL, sizeof(TelemetriaCompresor), PROT_READ, MAP_SHARED, shm_fd, 0);
    if (telemetria == MAP_FAILED) {
        perror("Error al mapear la memoria con mmap");
        close(shm_fd);
        exit(EXIT_FAILURE);
    }

    sem_t* sem = sem_open(SEM_NAME, 0);
    if (sem == SEM_FAILED) {
        perror("Error al abrir el semáforo con sem_open");
        munmap(telemetria, sizeof(TelemetriaCompresor));
        close(shm_fd);
        exit(EXIT_FAILURE);
    }

    printf("[MONITOR] Conexión establecida. Leyendo datos en tiempo real...\n\n");
    printf("--------------------------------------------------------------------------------\n");
    printf(" SECUENCIA | TIMESTAMP          | TEMP (°C) | PRES (Bar) | VIB (µm) | RPM   | ESTADO\n");
    printf("--------------------------------------------------------------------------------\n");

    uint64_t ultima_secuencia = 0;


    while (1 == 1) {
        sem_wait(sem);

        uint64_t seq = telemetria->numero_secuencia;
        time_t ts = telemetria->timestamp;
        float temp = telemetria->temperatura_cojinetes;
        float pres = telemetria->presion_descarga;
        float vib = telemetria->vibracion_axial;
        float rpm = telemetria->velocidad_rpm;
        int alerta = telemetria->estado_alerta;


        sem_post(sem);

        if (seq != ultima_secuencia) {
            ultima_secuencia = seq;

            struct tm* tiempolocal = localtime(&ts);
            char hora_str[9];
            strftime(hora_str, sizeof(hora_str), "%H:%M:%S", tiempolocal);

            printf(" %9lu | %s     | %9.1f | %10.1f | %8.1f | %5.0f | %s\n",
                seq,
                hora_str,
                temp,
                pres,
                vib,
                rpm,
                alerta == 0 ? "NORMAL" : "ALERTA");
        }
        usleep(500000);
    }

    printf("\n[MONITOR] Desconectando recursos locales...\n");

    sem_close(sem);
    munmap(telemetria, sizeof(TelemetriaCompresor));
    close(shm_fd);

    printf("[MONITOR] Salida limpia completada.\n");
    return 0;
}