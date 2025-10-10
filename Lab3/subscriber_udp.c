/*
 * subscriber_udp.c
 * Suscriptor UDP para el modelo publicación-suscripción
 * Compilar: gcc subscriber_udp.c -o subscriber_udp
 * Ejecutar: ./subscriber_udp 127.0.0.1 8080
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Uso: %s <IP_BROKER> <PUERTO>\n", argv[0]);
        exit(1);
    }

    char *ip = argv[1];
    int port = atoi(argv[2]);
    int sock;
    struct sockaddr_in broker_addr, local_addr;
    char topic[100], buffer[BUFFER_SIZE];
    socklen_t addr_len = sizeof(broker_addr);

    // Crear socket UDP
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Error al crear socket");
        exit(1);
    }

    // Enlazar a cualquier puerto local disponible
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = 0;

    if (bind(sock, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        perror("Error en bind");
        close(sock);
        exit(1);
    }

    // Configurar dirección del broker
    memset(&broker_addr, 0, sizeof(broker_addr));
    broker_addr.sin_family = AF_INET;
    broker_addr.sin_port = htons(port);
    broker_addr.sin_addr.s_addr = inet_addr(ip);

    printf("Ingrese el partido o tema al que desea suscribirse: ");
    fgets(topic, sizeof(topic), stdin);
    topic[strcspn(topic, "\n")] = '\0';

    // Enviar suscripción al broker
    char subscribe_msg[120];
    sprintf(subscribe_msg, "SUBSCRIBE %s", topic);
    sendto(sock, subscribe_msg, strlen(subscribe_msg), 0,
        (struct sockaddr *)&broker_addr, addr_len);
    printf("Suscripción enviada al broker UDP %s:%d\n", ip, port);

    printf("Esperando mensajes...\n");
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes = recvfrom(sock, buffer, BUFFER_SIZE - 1, 0, NULL, NULL);
        if (bytes > 0)
            printf("Mensaje recibido: %s\n", buffer);
    }

    close(sock);
    return 0;
}
