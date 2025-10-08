#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8081
#define BUFFER_SIZE 1024

int main() {
    int sock;
    struct sockaddr_in server_addr;
    char message[BUFFER_SIZE];
    
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Error al crear socket UDP");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    printf("✅ Listo para enviar mensajes UDP al broker (%s:%d)\n", SERVER_IP, SERVER_PORT);

    while (1) {
        printf("> ");
        fgets(message, BUFFER_SIZE, stdin);
        message[strcspn(message, "\n")] = 0;

        if (strcmp(message, "exit") == 0)
            break;

        if (sendto(sock, message, strlen(message), 0,
                   (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("Error al enviar mensaje UDP");
            break;
        }
    }

    close(sock);
    printf("🔒 Conexión cerrada.\n");
    return 0;
}
