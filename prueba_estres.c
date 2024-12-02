#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Función para realizar una prueba de carga utilizando Netcat
void prueba_de_estres_network(const char* ip, int puerto_abierto, int puerto_cerrado, int num_solicitudes) {
    char comando[256];

    for (int i = 0; i < num_solicitudes; i++) {
        snprintf(comando, sizeof(comando), "nc -zv %s %d", ip, puerto_abierto);
        int result_open = system(comando);

        if (result_open == 0) {
            printf("Solicitud exitosa al puerto %d de %s\n", puerto_abierto, ip);
        } else {
            printf("Error al intentar conectar al puerto %d de %s (fallo)\n", puerto_abierto, ip);
        }

        snprintf(comando, sizeof(comando), "nc -zv %s %d", ip, puerto_cerrado);
        int result_closed = system(comando);

        if (result_closed == 0) {
            printf("Solicitud exitosa al puerto %d de %s (esto debería fallar)\n", puerto_cerrado, ip);
        } else {
            printf("Error al intentar conectar al puerto %d de %s (fallo esperado)\n", puerto_cerrado, ip);
        }
    }
}

int main() {
    const char* ip = "192.168.1.1";
    int puerto_abierto = 80;
    int puerto_cerrado = 9999;
    int num_solicitudes = 10;

    prueba_de_estres_network(ip, puerto_abierto, puerto_cerrado, num_solicitudes);

    return 0;
}
