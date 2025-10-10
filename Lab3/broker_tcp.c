/*
 * broker_tcp.c
 *
 * Broker TCP para Linux (POSIX).
 * - Recibe mensajes de publishers y los reenvía a los subscribers suscritos.
 * - Formato de mensaje esperado:
 *      SUBSCRIBE <TOPIC>
 *      PUBLISH <TOPIC> <mensaje>
 *
 * Compilar:
 *   gcc broker_tcp.c -o broker_tcp
 * Ejecutar:
 *   ./broker_tcp
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <unistd.h>
 #include <errno.h>
 #include <arpa/inet.h>
 #include <sys/socket.h>
 #include <sys/types.h>
 #include <sys/select.h>
 
 #define PORT 8080
 #define MAX_CLIENTS 50
 #define BUFFER_SIZE 1024
 #define MAX_TOPICS 50
 
 /*Creamos una estructura para los topics*/
 typedef struct {
     char topic[50];
     int subscribers[MAX_CLIENTS];
     int num_subscribers;
 } Topic;
 
 Topic topics[MAX_TOPICS];
 int num_topics = 0;
 
 void process_message(char *message, int sender_fd);
 void subscribe_to_topic(char *topic, int fd);
 void publish_to_topic(char *topic, char *message);
 
 /* --- Función principal --- */
 int main() {
     int server_fd, new_socket, client_sockets[MAX_CLIENTS];
     struct sockaddr_in address;
     int addrlen = sizeof(address);
     fd_set readfds;
 
     /*Creamos el socket*/
     server_fd = socket(AF_INET, SOCK_STREAM, 0);
     if (server_fd < 0) {
         perror("socket failed");
         exit(EXIT_FAILURE);
     }
 
     int opt = 1;
     setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
 
     address.sin_family = AF_INET;
     address.sin_addr.s_addr = INADDR_ANY;
     address.sin_port = htons(PORT);
 
     if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
         perror("bind failed");
         exit(EXIT_FAILURE);
     }
 
     if (listen(server_fd, 5) < 0) {
         perror("listen");
         exit(EXIT_FAILURE);
     }
 
     printf("Broker TCP escuchando en puerto %d...\n", PORT);
 
     /*Inicializamos la lista de clientes*/
     for (int i = 0; i < MAX_CLIENTS; i++) client_sockets[i] = 0;
 
     char buffer[BUFFER_SIZE];
 
     /*Bucle principal del broker*/
     while (1) {
         FD_ZERO(&readfds);
         FD_SET(server_fd, &readfds);
         int max_sd = server_fd;
 
         for (int i = 0; i < MAX_CLIENTS; i++) {
             int sd = client_sockets[i];
             if (sd > 0) FD_SET(sd, &readfds);
             if (sd > max_sd) max_sd = sd;
         }
 
         int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
         if (activity < 0 && errno != EINTR) {
             perror("select error");
         }
 
         /*Nueva conexión*/
         if (FD_ISSET(server_fd, &readfds)) {
             new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
             if (new_socket < 0) {
                 perror("accept");
                 continue;
             }
 
             printf("Nueva conexión: fd=%d, ip=%s, puerto=%d\n",
                    new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));
 
             for (int i = 0; i < MAX_CLIENTS; i++) {
                 if (client_sockets[i] == 0) {
                     client_sockets[i] = new_socket;
                     break;
                 }
             }
         }
 
         // Mensajes de clientes existentes
         for (int i = 0; i < MAX_CLIENTS; i++) {
             int sd = client_sockets[i];
             if (sd > 0 && FD_ISSET(sd, &readfds)) {
                 int valread = read(sd, buffer, BUFFER_SIZE - 1);
                 if (valread <= 0) {
                     getpeername(sd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
                     printf("Cliente desconectado: %s:%d\n",
                            inet_ntoa(address.sin_addr), ntohs(address.sin_port));
                     close(sd);
                     client_sockets[i] = 0;
                 } else {
                     buffer[valread] = '\0';
                     printf("Mensaje recibido: %s\n", buffer);
                     process_message(buffer, sd);
                 }
             }
         }
     }
 
     close(server_fd);
     return 0;
 }
 
    /*Procesar mensajes entrantes*/
 void process_message(char *message, int sender_fd) {
     char command[20], topic[50], content[BUFFER_SIZE];
     memset(command, 0, sizeof(command));
     memset(topic, 0, sizeof(topic));
     memset(content, 0, sizeof(content));
 
     sscanf(message, "%s %s %[^\n]", command, topic, content);
 
     if (strcmp(command, "SUBSCRIBE") == 0) {
         subscribe_to_topic(topic, sender_fd);
     } else if (strcmp(command, "PUBLISH") == 0) {
         publish_to_topic(topic, content);
     } else {
         printf("Comando desconocido o formato inválido: %s\n", command);
     }
 }
 
 /* --- Suscribirse a un tema --- */
 void subscribe_to_topic(char *topic, int fd) {
     for (int i = 0; i < num_topics; i++) {
         if (strcmp(topics[i].topic, topic) == 0) {
             topics[i].subscribers[topics[i].num_subscribers++] = fd;
             printf("Nuevo suscriptor al tema %s\n", topic);
             return;
         }
     }
 
     // Si no existe, crear nuevo tema
     strcpy(topics[num_topics].topic, topic);
     topics[num_topics].subscribers[0] = fd;
     topics[num_topics].num_subscribers = 1;
     num_topics++;
 
     printf("Tema creado y suscriptor agregado: %s\n", topic);
 }
 
 /* --- Publicar mensaje a un tema --- */
 void publish_to_topic(char *topic, char *message) {
     for (int i = 0; i < num_topics; i++) {
         if (strcmp(topics[i].topic, topic) == 0) {
             for (int j = 0; j < topics[i].num_subscribers; j++) {
                 int sub_fd = topics[i].subscribers[j];
                 send(sub_fd, message, strlen(message), 0);
             }
             printf("Mensaje enviado a %d suscriptores del tema %s\n",
                    topics[i].num_subscribers, topic);
             return;
         }
     }
 
     printf("Tema no encontrado: %s\n", topic);
 }
 