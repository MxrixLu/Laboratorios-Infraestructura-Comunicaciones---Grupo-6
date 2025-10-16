# Laboratorio 3 - Sistema Pub/Sub con TCP y UDP

## Librerías Usadas

Implementamos un sistema de publicación-suscripción con TCP y UDP. Acá explicamos las librerías que usamos y para qué sirve cada una.

### 1. `<unistd.h>`
Esta es la librería básica de UNIX para llamadas al sistema. La usamos para:
- `close()` - Cerrar sockets cuando terminamos de usarlos (si no hacemos esto, el sistema se queda sin file descriptors)
- `read()` - Leer datos del socket en el broker TCP
- `sleep()` - Hacer pausas entre mensajes del publisher para simular un flujo realista

---

### 2. `<sys/socket.h>`
La librería más importante para trabajar con redes. Sin esta no hay sockets, y sin sockets no hay comunicación. 

Funciones clave:
- `socket()` - Crea el socket (TCP con `SOCK_STREAM` o UDP con `SOCK_DGRAM`)
- `bind()` - Asocia el socket a un puerto específico (lo usa el broker para escuchar)
- `listen()` y `accept()` - Para el servidor TCP: marca el socket como "servidor" y acepta conexiones
- `connect()` - Los clientes la usan para conectarse al broker
- `send()` / `recv()` - Para enviar y recibir en TCP
- `sendto()` / `recvfrom()` - Para enviar y recibir en UDP (necesitan la dirección destino cada vez)
- `setsockopt()` - Configura opciones (ej: `SO_REUSEADDR` para reusar puertos rápido en desarrollo)

No hay forma de hacer networking en C sin esta librería.

---

### 3. `<arpa/inet.h>`
Convierte entre formatos que los humanos entendemos y lo que la red necesita.

- `htons()` - Convierte el puerto al formato de red (big-endian). Necesario porque diferentes CPUs ordenan los bytes diferente
- `inet_addr()` / `inet_pton()` - Convierten IPs de texto ("127.0.0.1") a formato binario que usa la red
- `inet_ntoa()` - Al revés: de binario a texto (útil para logs)

Básicamente, nosotros escribimos "192.168.1.1" pero la red maneja 32 bits. Esta librería hace la traducción.

---

### 4. `<netinet/in.h>`
Define las estructuras para manejar direcciones IP y puertos.

Lo más importante:
- `struct sockaddr_in` - Estructura que guarda IP + puerto + familia de direcciones (IPv4)
- `AF_INET` - Constante que indica "estamos usando IPv4"
- `INADDR_ANY` - Dirección comodín (0.0.0.0) para escuchar en todas las interfaces de red

Sin esto no podríamos decirle al socket "conectate a tal IP en tal puerto".

---

### 5. `<sys/types.h>`
Define tipos de datos que usan las llamadas al sistema. Por ejemplo:
- `socklen_t` - Para tamaños de estructuras sockaddr
- `ssize_t` - Para valores que pueden ser negativos (indicando errores)

Básicamente asegura que el código funcione igual en diferentes sistemas UNIX/Linux.

---

### 6. `<sys/select.h>` - La más importante para el broker TCP
Esta es clave para manejar múltiples clientes a la vez sin usar threads.

El problema: el broker necesita atender varios clientes simultáneamente. Si usamos solo `recv()` o `accept()`, el programa se queda esperando en un solo socket.

La solución: `select()` monitorea múltiples sockets y avisa cuál tiene actividad.

Funciones:
- `select()` - Se queda esperando hasta que ALGÚN socket tenga actividad
- `FD_ZERO()`, `FD_SET()`, `FD_ISSET()` - Para manejar conjuntos de sockets

Cómo funciona en el broker:
```c
while (1) {
    FD_ZERO(&readfds);
    FD_SET(server_fd, &readfds);  // Socket que escucha nuevas conexiones
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] > 0)
            FD_SET(client_sockets[i], &readfds);  // Cada cliente conectado
    }
    
    select(...);  // Espera a que CUALQUIERA tenga actividad
    
    // Revisar cuál socket tiene actividad y procesarlo
}
```

Hay alternativas como `poll()` o threads, pero `select()` es estándar y suficiente para este lab.

---

### 7. `<errno.h>`
Para manejar errores del sistema de forma inteligente.

- `errno` - Variable global con el código del último error
- `EINTR` - Error que significa "te interrumpí pero no es grave"

Ejemplo: si haces Ctrl+C mientras `select()` está esperando, retorna -1 con `errno = EINTR`. No es un error real, solo una interrupción. Sin checkear esto, el programa se cerraría innecesariamente.

---

## Resumen

Todas estas librerías son estándar POSIX - no hay dependencias externas raras. Cada una cumple una función específica que no se puede omitir:

- `unistd.h` - Para cerrar sockets y evitar memory leaks
- Librerías de sockets - La única forma de hacer networking en C
- `sys/select.h` - Para manejar múltiples clientes sin threads
- `errno.h` - Para no confundir errores reales con interrupciones normales

Podríamos haber usado librerías de más alto nivel, pero el punto del lab era entender cómo funciona la capa de transporte desde abajo. Por eso trabajamos directo con sockets POSIX.

Las principales diferencias que implementamos:
- **TCP**: Orientado a conexión, necesita `listen()`, `accept()`, `connect()`. Usa `select()` para multiplexar clientes
- **UDP**: Sin conexión, usa `sendto()` / `recvfrom()` con direcciones explícitas en cada mensaje
