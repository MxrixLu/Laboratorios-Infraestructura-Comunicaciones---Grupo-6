/*
broker_tcp.c

Broker TCP para Linux (POSIX).
- Recibe mensajes TCP de publishers y los reenvía a todos los subscribers.
- Formato de mensaje: "PUB|TOPIC|mensaje\n"

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>

#define PORT 8080
#define MAX_CLIENTS 50
#define BUFFER_SIZE 1024
#define MAX_TOPICS 50


/* Para definir que subscriptores estan interesados en que temas, 
    realizamos una estructura la cual tendrá: 
    - un aray de topics
    - un arreglo de subscribers
    - un entero que cuenta el numero de subscribers
*/

typedef struct {
    char topic[50];
    int subscribers[MAX_CLIENTS];
    int num_subscribers;
} Topic;

Topic topics[MAX_TOPICS];
int num_topics = 0;

/* Ahora crearemos el socket del servidor*/

int server_fd, new_socket, client_sockets[MAX_CLIENTS];
struct sockaddr_in address;
int addrlen = sizeof(address);
fd_set readfds;

server_fd = socker(AF_INET, SOCK_STREAM, 0);
if (server_fd == 0) {
    perror("socket failed");
    exit(EXIT_FAILURE);
}

int opt = 1; 
setcokopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

address.sin_family = AF_INET;
address.sin_addr.s_addr = INADDR_ANY;
adress.sin_port = htons(PORT);

if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
}

if (listen(server_fd, 5) < 0) {
    perror("listen");
    exit(EXIT_FAILURE);
}

/* Aqui esta el broker escuchando conexiones en el puerto 8080*/

for (int i = 0; i < MAX_CLIENTS; i++) {
    client_sockets[i] = 0; /*Aqui inicializamos el array de clientes*/
}


char buffer[BUFFER_SIZE];
while(1){
    FD_ZERO(&readfds);
    FD_SET(server_fd, &readfds);
    int max_sd = server_fd;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] > 0) {
            FD_SET(client_sockets[i], &readfds);
        }
        if (client_sockets[i] > max_sd) {
            max_sd = client_sockets[i];
        }
    }


    int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
    if (activity < 0 && errno != EINTR) {
        perror("select error");
    }

    /* Ahora para una nueva conexWion*/
    if (FD_ISSET(server_fd, &readfds)) {
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        printf("New connection, socket fd is %d, ip is: %s, port: %d\n", new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_sockets[i] == 0) {
                client_sockets[i] = new_socket;
                printf("Adding to list of sockets as %d\n", i);
                break;
            }
        }
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        int sd = client_sockets[i];
        if (FD_ISSET(sd, &readfds)) {
            int valread = read(sd, buffer, BUFFER_SIZE);
            if (valread == 0) {
                // Desconexión
                getpeername(sd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
                printf("Cliente desconectado: ip %s, port %d\n",
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
/* Para proccesar los mensajes para ver si es una suscripción o publicación creamos:*/
void process_message(char *message, int sd) {
    char command[20], topic[50], content[BUFFER_SIZE];

    sscanf(message, "%s %s %[^\n]", command, topic, content);

    if (strcmp(command, "PUB") == 0) {
        subscribe_to_topic(topic, sd);
    } else if (strcmp(command, "SUB") == 0) {
        publish_to_topic(topic, content);
    }else{
        printf("Comando invalido\n");
    }
}

void subscribe_to_topic(char *topic, int sd) {
    /*Para suscribirse a un tema*/
    for (int i = 0; i < num_topics; i++) {
        if (strcmp(topics[i].topic, topic) == 0) {
            topics[i].subscribers[topics[i].num_subscribers++] = sd;
        }
    }

    strcpy(topics[num_topics].topic, topic);
    topics[num_topics].subscribers[0] = sd;
    topics[num_topics].sub_count = 1;
    topic_count++;
}

void publish_to_topic(char *topic, char *content) {
    for (int i = 0; i < num_topics; i++) {
        if (strcmp(topics[i].topic, topic) == 0) {
            for (int j = 0; j < topics[i].sub_count; j++) {
                send(topics[i].subscribers[j], content, strlen(content), 0);
            }
        }
    }
}



