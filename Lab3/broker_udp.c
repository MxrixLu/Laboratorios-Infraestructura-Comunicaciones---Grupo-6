/*
broker_udp.c

Broker UDP para Linux (POSIX).
- Recibe mensajes UDP de publishers y los reenvía a todos los subscribers.
- Formato de mensaje: "PUB|TOPIC|mensaje\n"

*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8081
#define BUFFER_SIZE 1024
#define MAX_TOPICS 20
#define MAX_SUBS 50

/*Creamos una estructura para los topics*/
typedef struct {
    char topic[50];
    struct sockaddr_in subscribers[MAX_SUBS];
    int sub_count;
} Topic;

Topic topics[MAX_TOPICS];
int topic_count = 0;

/* agregamos función para agregar un suscriptor a un topic*/
void add_subscriber(char *topic, struct sockaddr_in addr) {
    for (int i = 0; i < topic_count; i++) {
        if (strcmp(topics[i].topic, topic) == 0) {
            topics[i].subscribers[topics[i].sub_count++] = addr;
            printf("Nuevo suscriptor agregado al tema %s\n", topic);
            return;
        }
    }

    // Si el tema no existe, crearlo
    strcpy(topics[topic_count].topic, topic);
    topics[topic_count].subscribers[0] = addr;
    topics[topic_count].sub_count = 1;
    topic_count++;
    printf("Tema creado y suscriptor agregado: %s\n", topic);
}

/* Funcion para publicar mensajes a todos los suscriptores de un topic*/
void publish_message(char *topic, char *msg, int sockfd) {
    for (int i = 0; i < topic_count; i++) {
        if (strcmp(topics[i].topic, topic) == 0) {
            for (int j = 0; j < topics[i].sub_count; j++) {
                sendto(sockfd, msg, strlen(msg), 0,
                       (struct sockaddr *)&topics[i].subscribers[j],
                       sizeof(topics[i].subscribers[j]));
            }
            printf("📤 Mensaje enviado a %d suscriptores del tema %s\n",
                   topics[i].sub_count, topic);
            return;
        }
    }

    printf("No hay suscriptores para el tema %s\n", topic);
}

int main() {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    char buffer[BUFFER_SIZE];
    socklen_t addr_len = sizeof(client_addr);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Error al crear socket UDP");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error en bind");
        close(sockfd);
        exit(1);
    }

    printf("Broker UDP escuchando en el puerto %d...\n", PORT);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0,
                             (struct sockaddr *)&client_addr, &addr_len);
        if (bytes < 0) {
            perror("Error al recibir");
            continue;
        }

        buffer[bytes] = '\0';
        printf("Mensaje recibido: %s\n", buffer);

        char command[20], topic[50], msg[BUFFER_SIZE];
        sscanf(buffer, "%s %s %[^\n]", command, topic, msg);

        if (strcmp(command, "SUBSCRIBE") == 0) {
            add_subscriber(topic, client_addr);
        } else if (strcmp(command, "PUBLISH") == 0) {
            publish_message(topic, msg, sockfd);
        } else {
            printf("Comando desconocido: %s\n", command);
        }
    }

    close(sockfd);
    return 0;
}
