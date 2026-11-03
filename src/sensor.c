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
    //shm_open es una llamada al sistema para crear regiones de memoria compartida
    // 
    //le pasamos el alias definido en common.h SHM_NAME
    // 
    //  O_CREAT | O_RDWR le dice que si nadie creo una region de memoria compartida que la cree , y si alguien la creo
    // que se use normalmente , en la la misma se podra leer y escribir
    // 
    //creacion del objeto , istanciamos la llamada a POSIX , esta nos devolvera un descriptor de archivo que sera un entero

    //el descriptor de archivo actúa como la llave de acceso única para interactuar con esa región de memoria compartida.
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);

    //al ser un entero lo que devuelve la istancia de shm_open , si devuelve -1 es porque hubo un error
    if (shm_fd == -1) {
        perror("Error al ejecutar shm_open");
        exit(EXIT_FAILURE);
    }

    //// le ordenamos al kernel que el tamaño de la memoria compartida sea del mismo tamaño que soporta como maximo 
    // TelemetriaCompresor 
    if (ftruncate(shm_fd, sizeof(TelemetriaCompresor)) == -1) {
        perror("Error al configurar el tamaño con ftruncate");
        close(shm_fd);
        exit(EXIT_FAILURE);
    }

    //Puntero hacia la direccion de memoria compartida el mismo tiene el tipo TelemetriaCompresor
    TelemetriaCompresor* telemetria = mmap(NULL, sizeof(TelemetriaCompresor), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (telemetria == MAP_FAILED) {
        perror("Error al mapear la memoria con mmap");
        close(shm_fd);
        exit(EXIT_FAILURE);
    }


    //sem_t es una estructura de datos interna de Posix y el kernel de linux
    //este mismo con tiene todos los campos que se necesitan para gestionar un semaforo en linux

    //sem_t* sem , sem es un puntero del tipo sem_t que apunta hacia

    //sem_open(SEM_NAME, O_CREAT | O_RDWR, 0666, 1); retorna una direccion de memoria , aqui podremos usar sem_wait y sem_post
    // si ocurrio un error retorna SEM_FAILED
    
    sem_t* sem = sem_open(SEM_NAME, O_CREAT | O_RDWR, 0666, 1);
    if (sem == SEM_FAILED) {
        perror("Error al inicializar el semáforo con sem_open");
        munmap(telemetria, sizeof(TelemetriaCompresor));
        close(shm_fd);
        exit(EXIT_FAILURE);
    }

    printf("[SENSOR] Memoria compartida y semáforos listos. Iniciando transmisión...\n\n");
    printf("------------------------------------------------------------------------------------------------\n");
    printf(" SECUENCIA | FECHA Y HORA       | TEMP (°C) | PRES (Bar) | VIB (µm) | RPM   | ESTADO\n");
    printf("------------------------------------------------------------------------------------------------\n");

    uint64_t secuencia = 0;

    while (keep_running) {
        time_t t_actual = time(NULL);
        struct tm* tiempo_info = localtime(&t_actual);

        sem_wait(sem);

        telemetria->numero_secuencia = ++secuencia;
        telemetria->timestamp = t_actual;

        // Rellenamos la fecha y hora desagregada
        telemetria->anio = tiempo_info->tm_year + 1900;
        telemetria->mes = tiempo_info->tm_mon + 1;
        telemetria->dia = tiempo_info->tm_mday;
        telemetria->hora = tiempo_info->tm_hour;
        telemetria->minuto = tiempo_info->tm_min;
        telemetria->segundo = tiempo_info->tm_sec;

        // Métricas físicas simuladas
        telemetria->temperatura_cojinetes = 65.5f;
        telemetria->presion_descarga = 18.2f;
        telemetria->vibracion_axial = 22.0f;
        telemetria->velocidad_rpm = 9500.0f;
        telemetria->estado_alerta = 0;

        sem_post(sem);

        // Formateamos la fecha y hora para la consola idéntica al monitor
        char fecha_hora_str[20];
        snprintf(fecha_hora_str, sizeof(fecha_hora_str), "%04d-%02d-%02d %02d:%02d:%02d",
            telemetria->anio, telemetria->mes, telemetria->dia,
            telemetria->hora, telemetria->minuto, telemetria->segundo);

        printf(" %9lu | %s | %9.1f | %10.1f | %8.1f | %5.0f | %s\n",
            secuencia,
            fecha_hora_str,
            telemetria->temperatura_cojinetes,
            telemetria->presion_descarga,
            telemetria->vibracion_axial,
            telemetria->velocidad_rpm,
            telemetria->estado_alerta == 0 ? "NORMAL" : "ALERTA");

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