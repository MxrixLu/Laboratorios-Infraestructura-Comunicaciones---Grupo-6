/*
broker_udp.c

Broker UDP para Linux (POSIX).
- Recibe mensajes UDP de publishers y los reenvía a todos los subscribers.
- Formato de mensaje: "PUB|TOPIC|mensaje\n"

*/