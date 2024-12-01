#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define NUM_REQUESTS 1000
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 3000


/**
 * Prueba de estrés para nc.
 */

void *hacer_solicitud(void *arg) {
    int sockfd;
    struct sockaddr_in server_addr;
    char *message = "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
    char buffer[1024];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Error al crear el socket");
        exit(1);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error al conectar con el servidor");
        close(sockfd);
        exit(1);
    }

    send(sockfd, message, strlen(message), 0);

    int bytes_read = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
    }

    close(sockfd);
    return NULL;
}

int main() {
    pthread_t hilos[NUM_REQUESTS];

    for (int i = 0; i < NUM_REQUESTS; i++) {
        if (pthread_create(&hilos[i], NULL, hacer_solicitud, NULL) != 0) {
            perror("Error al crear el hilo");
            exit(1);
        }
    }

    for (int i = 0; i < NUM_REQUESTS; i++) {
        pthread_join(hilos[i], NULL);
    }

    printf("Prueba de carga terminada: %d solicitudes realizadas.\n", NUM_REQUESTS);
    return 0;
}
