#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <signal.h>
#include <string.h>
#include <time.h>

#include "common.h"

#define LECTURAS 1000000 // Cantidad máxima de lecturas

// Variable volátil para captura de señal SIGINT (Ctrl+C)
static volatile sig_atomic_t corriendo = 1;

static void cortar(int s) {
    (void)s;
    corriendo = 0;
}

int main(void) {
    // Captura de Ctrl+C (SIGINT) para finalización ordenada
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = cortar;
    sigaction(SIGINT, &sa, NULL);

    printf("[MONITOR] Conectándose al sistema de telemetría del compresor...\n");

    // 1. Abrir la memoria compartida
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Error al abrir shm_open (¿Está el sensor encendido?)");
        exit(EXIT_FAILURE);
    }

    // 2. Mapear memoria
    TelemetriaCompresor* telemetria = mmap(NULL, sizeof(TelemetriaCompresor),
        PROT_READ | PROT_WRITE, MAP_SHARED,
        shm_fd, 0);
    if (telemetria == MAP_FAILED) {
        perror("Error al mapear la memoria con mmap");
        close(shm_fd);
        exit(EXIT_FAILURE);
    }

    printf("[MONITOR] Conexión establecida. Leyendo datos en tiempo real...\n");
    printf("Ctrl+C para finalizar y ver el reporte completo.\n\n");
    printf("------------------------------------------------------------------------------------------------\n");
    printf(" SECUENCIA | FECHA Y HORA         | TEMP (°C) | PRES (Bar) | VIB (µm) | RPM   | ESTADO\n");
    printf("------------------------------------------------------------------------------------------------\n");

    // Contadores de métricas
    unsigned long inconsistentes = 0;
    unsigned long alertas = 0;
    unsigned long distintas = 0;
    unsigned long ultima_secuencia = 0;
    unsigned long ultimo_corrupto = 0;
    unsigned long mediciones_corruptas = 0;
    int lecturas_realizadas = 0;

    // El bucle se detiene al llegar a LECTURAS o al presionar Ctrl+C (corriendo == 0)
    for (int n = 0; n < LECTURAS && corriendo; n++) {
        lecturas_realizadas++;
        TelemetriaCompresor c;

        // --- SECCIÓN CRÍTICA ---
        LOCK(telemetria);
        c = *telemetria;
        UNLOCK(telemetria);

        // 1. Nueva medición detectada
        if (c.numero_secuencia != ultima_secuencia) {
            distintas++;
            ultima_secuencia = c.numero_secuencia;

            char fecha_hora_str[32];
            snprintf(fecha_hora_str, sizeof(fecha_hora_str), "%04d-%02d-%02d %02d:%02d:%02d",
                c.anio, c.mes, c.dia, c.hora, c.minuto, c.segundo);

            printf(" %9lu | %s | %9.1f | %10.1f | %8.1f | %5.0f | %s\n",
                c.numero_secuencia,
                fecha_hora_str,
                c.temperatura_cojinetes,
                c.presion_descarga,
                c.vibracion_axial,
                c.velocidad_rpm,
                c.estado_alerta == 0 ? "NORMAL" : "ALERTA");
        }

        // 2. Control de consistencia matemática (presión = 2*temp, vibración = 3*temp)
        if (c.presion_descarga != (c.temperatura_cojinetes * 2.0f) ||
            c.vibracion_axial != (c.temperatura_cojinetes * 3.0f)) {

            inconsistentes++;

            if (c.numero_secuencia != ultimo_corrupto) {
                mediciones_corruptas++;
                ultimo_corrupto = c.numero_secuencia;

                if (mediciones_corruptas <= 5) {
                    printf("  [!] LECTURA CORRUPTA DETECTADA: seq=%lu | temp=%.1f | pres=%.1f | vib=%.1f\n",
                        c.numero_secuencia, c.temperatura_cojinetes, c.presion_descarga, c.vibracion_axial);
                }
            }
        }
        // 3. Control de alertas críticas
        else if (c.temperatura_cojinetes > TEMP_CRITICA || c.estado_alerta != 0) {
            alertas++;
        }

        usleep(100);
    }

    // --- REPORTE ESTADÍSTICO FINAL ---
    printf("\n================================================================================================\n");
    printf("RESUMEN DE MONITORIZACIÓN Y AUDITORÍA:\n");
    printf("  Lecturas totales realizadas:     %d\n", lecturas_realizadas);
    printf("  Mediciones distintas capturadas: %lu\n", distintas);
    printf("  Lecturas inconsistentes:         %lu (%.4f%%)\n",
        inconsistentes, lecturas_realizadas > 0 ? (100.0 * (double)inconsistentes / lecturas_realizadas) : 0.0);
    printf("  Mediciones corruptas únicas:     %lu de %lu\n", mediciones_corruptas, distintas);
    printf("  Alertas de peligro detectadas:   %lu\n", alertas);
    printf("================================================================================================\n");

    printf("\n[MONITOR] Desconectando recursos locales...\n");
    munmap(telemetria, sizeof(TelemetriaCompresor));
    close(shm_fd);

    printf("[MONITOR] Salida limpia completada.\n");
    return 0;
}