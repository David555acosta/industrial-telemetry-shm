#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <time.h>
#include <semaphore.h> // Incluido para reconocer sem_t

#define SHM_NAME "/shm_telemetria_compresor"

#define TEMP_CRITICA 90.0f
#define PRESION_CRITICA 25.0f
#define VIBRACION_CRITICA 60.0f
#define RPM_MINIMA 6000.0f
#define RPM_MAXIMA 13000.0f

// Sintaxis corregida agregando {
typedef struct {
    sem_t mutex;

    uint64_t numero_secuencia;
    time_t timestamp;

    int anio;
    int mes;
    int dia;
    int hora;
    int minuto;
    int segundo;

    float temperatura_cojinetes;
    float presion_descarga;
    float vibracion_axial;
    float velocidad_rpm;

    int estado_alerta;
} TelemetriaCompresor;

#ifdef SIN_SYNC
#define LOCK(t)
#define UNLOCK(t)
#else
#define LOCK(t) sem_wait(&(t)->mutex)
#define UNLOCK(t) sem_post(&(t)->mutex)
#endif

#endif // COMMON_H