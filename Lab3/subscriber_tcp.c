/*
 * subscriber_tcp.c
 * Suscriptor TCP para el modelo publicación-suscripción
 * Compilar: gcc subscriber_tcp.c -o subscriber_tcp
 * Ejecutar: ./subscriber_tcp 127.0.0.1 8080
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    sleep(1);
    if (argc != 3) {
        printf("Uso: %s <IP_BROKER> <PUERTO>\n", argv[0]);
        exit(1);
    }

    char *ip = argv[1];
    int port = atoi(argv[2]);
    int sock;
    struct sockaddr_in broker_addr;
    char topic[100], buffer[BUFFER_SIZE];

    // Crear socket TCP
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Error al crear socket");
        exit(1);
    }

    broker_addr.sin_family = AF_INET;
    broker_addr.sin_port = htons(port);
    broker_addr.sin_addr.s_addr = inet_addr(ip);

    // Conectar con el broker
    if (connect(sock, (struct sockaddr *)&broker_addr, sizeof(broker_addr)) < 0) {
        perror("Error al conectar con el broker");
        close(sock);
        exit(1);
    }

    printf("Conectado al broker TCP en %s:%d\n", ip, port);
    printf("Ingrese el partido o tema al que desea suscribirse: ");
    fgets(topic, sizeof(topic), stdin);
    topic[strcspn(topic, "\n")] = '\0';

    // Enviar tema al broker
    char msg[120];
    snprintf(msg, sizeof(msg), "SUBSCRIBE %s", topic);
    send(sock, msg, strlen(msg), 0);

    printf("Esperando mensajes...\n");
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes <= 0) {
            printf("Conexión cerrada por el broker.\n");
            break;
        }
        printf("Mensaje recibido: %s\n", buffer);
    }

    close(sock);
    return 0;
}
