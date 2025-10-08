/* publisher_tcp.c
 *
 * Publisher TCP para Linux (POSIX).
 * - Envía 10 mensajes automáticamente por defecto (simula un periodista).
 * - Formato de mensaje: "PUB|TOPIC|mensaje\n"
 *
 * Compilar:
 *   gcc -std=c11 publisher_tcp.c -o publisher_tcp
 *
 * Ejecutar:
 *   ./publisher_tcp <broker_ip> <broker_port> <TOPIC> <PUBLISHER_ID>
 * Ejemplo:
 *   ./publisher_tcp 127.0.0.1 5000 "A_vs_B" P1
 *
 * Para correr 2 publishers simultáneos: abrir 2 terminales y ejecutar con IDs distintos,
 * o ejecutar uno en background:
 *   ./publisher_tcp 127.0.0.1 5000 "A_vs_B" P1 &
 *   ./publisher_tcp 127.0.0.1 5000 "A_vs_B" P2 &
 */

#include <stdio.h>          // printf, perror
#include <stdlib.h>         // exit, atoi
#include <string.h>         // strlen, strcpy, strcat, memset, snprintf
#include <unistd.h>         // close, sleep
#include <sys/types.h>
#include <sys/socket.h>     // socket, connect, send
#include <netinet/in.h>     // struct sockaddr_in, htons
#include <arpa/inet.h>      // inet_pton

#define DEFAULT_MSGS 10
#define BUF_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Uso: %s <broker_ip> <broker_port> <TOPIC> <PUBLISHER_ID>\n", argv[0]);
        fprintf(stderr, "Ej: %s 127.0.0.1 5000 \"A_vs_B\" P1\n", argv[0]);
        return 1;
    }

    const char *broker_ip = argv[1];
    int broker_port = atoi(argv[2]);
    const char *topic = argv[3];
    const char *pub_id = argv[4];

    int sockfd;
    struct sockaddr_in broker_addr;

    /* 1) socket()
       Crea un descriptor de socket:
       - AF_INET : familia IPv4
       - SOCK_STREAM : TCP (stream, confiable)
       - protocolo 0 : el sistema elige TCP para SOCK_STREAM
       Retorna >=0 descriptor, o -1 en error (errno explicado por perror).
    */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket()");
        exit(EXIT_FAILURE);
    }

    /* 2) preparar struct sockaddr_in con IP y puerto */
    memset(&broker_addr, 0, sizeof(broker_addr));
    broker_addr.sin_family = AF_INET;
    broker_addr.sin_port = htons(broker_port); // htons convierte host->network (big-endian)

    /* inet_pton convierte texto IPv4 a binario en sin_addr.
       Retorna 1 si OK, 0 si formato inválido, -1 en error. */
    if (inet_pton(AF_INET, broker_ip, &broker_addr.sin_addr) != 1) {
        perror("inet_pton()");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    /* 3) connect()
       Establece conexión TCP con el broker (handshake TCP).
       Retorna 0 si éxito, -1 en error.
    */
    if (connect(sockfd, (struct sockaddr *)&broker_addr, sizeof(broker_addr)) < 0) {
        perror("connect()");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("[PUBLISHER %s] Conectado a %s:%d, topic=%s\n", pub_id, broker_ip, broker_port, topic);

    /* 4) Enviar N mensajes (DEFAULT_MSGS) con send()
       send(fd, buffer, len, flags):
       - retorna número de bytes enviados, o -1 en error.
       - con TCP, send() puede devolver menos bytes; aquí enviamos mensajes pequeños,
         normalmente se mandan completos.
    */
    char out[BUF_SIZE];
    for (int i = 1; i <= DEFAULT_MSGS; ++i) {
        // Construimos el mensaje según formato: PUB|TOPIC|[ID] mensaje i\n
        int n = snprintf(out, sizeof(out), "PUB|%s|[%s] mensaje %d\n", topic, pub_id, i);
        if (n < 0) {
            fprintf(stderr, "Error al formar el mensaje\n");
            break;
        }

        ssize_t sent = send(sockfd, out, (size_t)n, 0);
        if (sent < 0) {
            perror("send()");
            break;
        }

        printf("[PUBLISHER %s] Enviado (%zd bytes): %s", pub_id, sent, out);
        sleep(1); // simula tiempo entre eventos
    }

    /* 5) cerrar socket */
    close(sockfd);
    printf("[PUBLISHER %s] Terminado.\n", pub_id);
    return 0;
}
