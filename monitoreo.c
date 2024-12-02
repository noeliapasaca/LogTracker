#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <curl/curl.h>
#include <sys/wait.h>
#include <ctype.h>

// Variables globales para contar los logs por tipo
int count_default = 0;
int count_error = 0;
int count_fault = 0;
int count_info = 0;
int count_debug = 0;

#define UMBRAL_ERRORES 500

// Mutex para sincronizar el acceso a las variables globales
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int alerta_enviada = 0;

/**
 * Función para extraer un número de una cadena (buffer).
 */
int extraer_numero(const char *str) {
    int valor = 0;
    while (*str && isdigit(*str)) { 
        valor = valor * 10 + (*str - '0');
        str++;
    }
    return valor;
}

/**
 * Función para enviar una alerta a través de Twilio.
 * Llama a un script Python que se encarga de enviar el mensaje.
 */
void enviar_alerta() {
    int total_errores = count_error + count_fault;

    char comando[256];
    snprintf(comando, sizeof(comando), "python3 prueba_mensaje.py %d", total_errores);

    int result = system(comando);

    if (result == -1) {
        perror("Error al ejecutar prueba_mensaje.py");
    }
}

/**
 * Función para leer logs en tiempo real de un servicio especificado.
 * Usa 'log stream' para obtener los logs, los cuenta por tipo y los envía al proceso padre a través del pipe.
 */
void leer_logs(const char* servicio, int write_fd) {
    FILE *fp;
    char path[1035];

    char comando[256];
    snprintf(comando, sizeof(comando), "log stream --predicate 'eventMessage contains \"%s\"' --info", servicio);
    fp = popen(comando, "r");
    if (fp == NULL) {
        perror("Error al ejecutar log stream");
        exit(1);
    }

    while (fgets(path, sizeof(path)-1, fp) != NULL) {

        if (strstr(path, "Default") != NULL) count_default++;
        if (strstr(path, "Error") != NULL) count_error++;
        if (strstr(path, "Fault") != NULL) count_fault++;
        if (strstr(path, "Info") != NULL) count_info++;
        if (strstr(path, "Debug") != NULL) count_debug++;

        char buffer[256];
        snprintf(buffer, sizeof(buffer), "Servicio %s - Default: %d, Error: %d, Fault: %d, Info: %d, Debug: %d\n", 
                 servicio, count_default, count_error, count_fault, count_info, count_debug);

        write(write_fd, buffer, strlen(buffer));  
    }

    fclose(fp);
}

/**
 * Función para mostrar el dashboard en modo texto con los contadores de logs.
 * Muestra los valores actuales de los contadores y genera una alerta si los errores superan el umbral.
 */
void mostrar_dashboard() {
    printf("\n=== Dashboard de Logs ===\n");
    printf("Default (default): %d\n", count_default);
    printf("Errores (error): %d\n", count_error);
    printf("Faults (fault): %d\n", count_fault);
    printf("Información (info): %d\n", count_info);
    printf("Depuración (debug): %d\n", count_debug);
    printf("=========================\n");

    if (count_error + count_fault > UMBRAL_ERRORES) {
        if (!alerta_enviada) {
            enviar_alerta();
            alerta_enviada = 1;
        }
    } else {
        alerta_enviada = 0;
    }
}

/**
 * Función para actualizar los contadores con los valores extraídos del buffer.
 * Lee el buffer que contiene los logs y extrae los valores de los contadores (Default, Error, Fault, etc.).
 * Usamos sscanf para extraer los valores después de cada etiqueta.
 */
void actualizar_contadores(char* buffer) {
    int default_val = 0, error_val = 0, fault_val = 0, info_val = 0, debug_val = 0;

    char *ptr_default = strstr(buffer, "Default:");
    if (ptr_default != NULL) {
        sscanf(ptr_default + 8, "%d", &default_val);
    }

    char *ptr_error = strstr(buffer, "Error:");
    if (ptr_error != NULL) {
        sscanf(ptr_error + 6, "%d", &error_val);
    }

    char *ptr_fault = strstr(buffer, "Fault:");
    if (ptr_fault != NULL) {
        sscanf(ptr_fault + 6, "%d", &fault_val);
    }

    char *ptr_info = strstr(buffer, "Info:");
    if (ptr_info != NULL) {
        sscanf(ptr_info + 5, "%d", &info_val);
    }

    char *ptr_debug = strstr(buffer, "Debug:");
    if (ptr_debug != NULL) {
        sscanf(ptr_debug + 7, "%d", &debug_val);
    }

    pthread_mutex_lock(&mutex);

    count_default += default_val;
    count_error += error_val;
    count_fault += fault_val;
    count_info += info_val;
    count_debug += debug_val;

    pthread_mutex_unlock(&mutex); 
}

/**
 * Función para el proceso hijo que lee logs de un servicio y pasa la información al proceso padre.
 * Crea un pipe para la comunicación entre el proceso hijo y el proceso padre.
 */
void *hilo_monitoreo_servicio(void *arg) {
    const char *servicio = (const char *)arg;

    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        perror("Error al crear el pipe");
        exit(1);
    }

    pid_t pid = fork();

    if (pid == -1) {
        perror("Error al hacer fork");
        exit(1);
    }

    if (pid == 0) {
        close(pipe_fd[0]);
        leer_logs(servicio, pipe_fd[1]);
        close(pipe_fd[1]);
        exit(0);
    }

    close(pipe_fd[1]);

    char buffer[256];
    while (1) {
        ssize_t bytes_read = read(pipe_fd[0], buffer, sizeof(buffer) - 1);
        
        if (bytes_read < 0) {
            perror("Error al leer desde el pipe");
            exit(1);
        } else if (bytes_read == 0) {
            break;
        }

        buffer[bytes_read] = '\0';

        actualizar_contadores(buffer);
    }

    close(pipe_fd[0]);
    wait(NULL);

    return NULL;
}

/**
 * Función para el hilo que actualiza el dashboard en tiempo real.
 * Este hilo actualizará el dashboard a intervalos regulares.
 */
void *hilo_actualizacion_dashboard(void *arg) {
    while (1) {
        mostrar_dashboard();
        sleep(3);
    }
}

/**
 * Función principal del programa.
 * Toma dos parámetros para monitorear los logs de dos servicios en paralelo utilizando hilos y procesos hijos.
 */
int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Uso: %s <servicio1> <servicio2>\n", argv[0]);
        return 1;
    }
    printf("Servicios a monitorear: %s y %s\n", argv[1], argv[2]);

    pthread_t hilo_logs1, hilo_logs2, hilo_dashboard;

    pthread_create(&hilo_logs1, NULL, hilo_monitoreo_servicio, (void *)argv[1]);
    pthread_create(&hilo_logs2, NULL, hilo_monitoreo_servicio, (void *)argv[2]);

    pthread_create(&hilo_dashboard, NULL, hilo_actualizacion_dashboard, NULL);

    pthread_join(hilo_logs1, NULL);
    pthread_join(hilo_logs2, NULL);
    pthread_join(hilo_dashboard, NULL);

    return 0;
}
