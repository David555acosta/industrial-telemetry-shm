#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <time.h>

// Nombres únicos para los recursos POSIX en el Kernel
#define SHM_NAME "/shm_telemetria_compresor"
#define SEM_NAME "/sem_telemetria_compresor"

// Límites de Alerta (Umbrales Críticos)
#define TEMP_CRITICA 90.0f
#define PRESION_CRITICA 25.0f
#define VIBRACION_CRITICA 60.0f
#define RPM_MINIMA 6000.0f
#define RPM_MAXIMA 13000.0f

// Estructura que representa el estado instantáneo de la maquinaria
typedef struct {
    uint64_t numero_secuencia;    // Contador incremental para verificar continuidad
    time_t timestamp;             // Marca de tiempo UNIX

    // Métricas físicas del Compresor Centrífugo
    float temperatura_cojinetes;  // en °C (Normal: 50.0 - 80.0)
    float presion_descarga;       // en Bar (Normal: 15.0 - 22.0)
    float vibracion_axial;        // en micrómetros µm (Normal: 10.0 - 35.0)
    float velocidad_rpm;          // en RPM (Normal: 8,000 - 12,000)

    // Estado operativo (0 = Normal, 1 = Alerta Crítica por umbral superado)
    int estado_alerta;
} TelemetriaCompresor;

#endif // COMMON_H