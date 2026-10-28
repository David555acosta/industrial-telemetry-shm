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
//inclucion biblioteca que creamos donde alojamos la firma del struct , los umbrales maximos de cada campo del
// TelemetriaCompresor y los nombres unicos que le pasamos al semaforo y memoria compartida
volatile sig_atomic_t keep_running = 1;

void handle_sigint(int sig) {
    (void)sig;
    keep_running = 0;
}

int main(void) {


    signal(SIGINT, handle_sigint);

    printf("[SENSOR] Iniciando proceso emisor de telemetría...\n");

    
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Error al ejecutar shm_open");
        exit(EXIT_FAILURE);
    }

    
    if (ftruncate(shm_fd, sizeof(TelemetriaCompresor)) == -1) {
        perror("Error al configurar el tamaño con ftruncate");
        close(shm_fd);
        exit(EXIT_FAILURE);
    }

    
    TelemetriaCompresor* telemetria = mmap(NULL, sizeof(TelemetriaCompresor), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (telemetria == MAP_FAILED) {
        perror("Error al mapear la memoria con mmap");
        close(shm_fd);
        exit(EXIT_FAILURE);
    }

    
    sem_t* sem = sem_open(SEM_NAME, O_CREAT | O_RDWR, 0666, 1);
    if (sem == SEM_FAILED) {
        perror("Error al inicializar el semáforo con sem_open");
        munmap(telemetria, sizeof(TelemetriaCompresor));
        close(shm_fd);
        exit(EXIT_FAILURE);
    }

    printf("[SENSOR] Memoria compartida y semáforos listos. Iniciando transmisión...\n");

    uint64_t secuencia = 0;

    
    while (keep_running) {
        
        sem_wait(sem);

        telemetria->numero_secuencia = ++secuencia;
        telemetria->timestamp = time(NULL);

        
        telemetria->temperatura_cojinetes = 65.5f;
        telemetria->presion_descarga = 18.2f;
        telemetria->vibracion_axial = 22.0f;
        telemetria->velocidad_rpm = 9500.0f;
        telemetria->estado_alerta = 0;

        sem_post(sem);

        printf("[SENSOR] Dato enviado -> Secuencia: %lu | Temp: %.1f °C | RPM: %.1f\n",
            secuencia, telemetria->temperatura_cojinetes, telemetria->velocidad_rpm);


        sleep(1);
    }


    printf("\n[SENSOR] Apagando y liberando recursos del Kernel...\n");

    sem_close(sem);
    sem_unlink(SEM_NAME);

    munmap(telemetria, sizeof(TelemetriaCompresor));
    close(shm_fd);
    shm_unlink(SHM_NAME);

    printf("[SENSOR] Proceso finalizado correctamente.\n");
    return 0;
}