#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <msquic.h>

#define BUFFER_SIZE 2048

static const QUIC_API_TABLE *MsQuic = NULL;
static HQUIC Registration = NULL;
static HQUIC Configuration = NULL;

typedef struct {
    char buffer[BUFFER_SIZE];
    size_t length;
} StreamCtx;

static QUIC_STATUS QUIC_API StreamCallback(HQUIC Stream, void *Context, QUIC_STREAM_EVENT *Event) {
    (void)Context;
    switch (Event->Type) {
        case QUIC_STREAM_EVENT_RECEIVE: {
            for (uint32_t i = 0; i < Event->RECEIVE.BufferCount; i++) {
                fwrite(Event->RECEIVE.Buffers[i].Buffer, 1, Event->RECEIVE.Buffers[i].Length, stdout);
            }
            fflush(stdout);
            return QUIC_STATUS_SUCCESS;
        }
        case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
            MsQuic->StreamClose(Stream);
            return QUIC_STATUS_SUCCESS;
        default:
            return QUIC_STATUS_SUCCESS;
    }
}

static QUIC_STATUS QUIC_API ConnectionCallback(HQUIC Connection, void *Context, QUIC_CONNECTION_EVENT *Event) {
    (void)Context;
    switch (Event->Type) {
        case QUIC_CONNECTION_EVENT_CONNECTED:
            return QUIC_STATUS_SUCCESS;
        case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
            MsQuic->ConnectionClose(Connection);
            return QUIC_STATUS_SUCCESS;
        default:
            return QUIC_STATUS_SUCCESS;
    }
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <broker_ip> <port> <TOPIC>\n", argv[0]);
        return 1;
    }
    const char *server = argv[1];
    uint16_t port = (uint16_t)atoi(argv[2]);
    const char *topic = argv[3];

    if (MsQuicOpen(&MsQuic) != QUIC_STATUS_SUCCESS) return 1;
    QUIC_REGISTRATION_CONFIG regConfig = { "subscriber-quic", QUIC_EXECUTION_PROFILE_LOW_LATENCY };
    if (MsQuic->RegistrationOpen(&regConfig, &Registration) != QUIC_STATUS_SUCCESS) return 1;

    const char *alpn = "pubsub";
    QUIC_BUFFER alpnBuffer = { (uint32_t)strlen(alpn), (uint8_t *)alpn };
    QUIC_SETTINGS settings; memset(&settings, 0, sizeof(settings));
    if (MsQuic->ConfigurationOpen(Registration, &alpnBuffer, 1, &settings, sizeof(settings), NULL, &Configuration) != QUIC_STATUS_SUCCESS) return 1;

    QUIC_CREDENTIAL_CONFIG cred = {0};
    cred.Type = QUIC_CREDENTIAL_TYPE_NONE;
    cred.Flags = QUIC_CREDENTIAL_FLAG_CLIENT | QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION;
    if (MsQuic->ConfigurationLoadCredential(Configuration, &cred) != QUIC_STATUS_SUCCESS) return 1;

    HQUIC Connection = NULL;
    if (MsQuic->ConnectionOpen(Registration, ConnectionCallback, NULL, &Connection) != QUIC_STATUS_SUCCESS) return 1;
    if (MsQuic->ConnectionStart(Connection, Configuration, QUIC_ADDRESS_FAMILY_UNSPEC, server, port) != QUIC_STATUS_SUCCESS) return 1;

    HQUIC Stream = NULL;
    if (MsQuic->StreamOpen(Connection, QUIC_STREAM_OPEN_FLAG_NONE, StreamCallback, NULL, &Stream) != QUIC_STATUS_SUCCESS) return 1;
    if (MsQuic->StreamStart(Stream, QUIC_STREAM_START_FLAG_IMMEDIATE) != QUIC_STATUS_SUCCESS) return 1;

    char msg[256];
    int n = snprintf(msg, sizeof(msg), "SUBSCRIBE %s\n", topic);
    QUIC_BUFFER buf; buf.Length = (uint32_t)n; buf.Buffer = (uint8_t *)msg;
    MsQuic->StreamSend(Stream, &buf, 1, QUIC_SEND_FLAG_ALLOW_0_RTT, NULL);

    printf("Waiting for messages...\n");
    getchar();

    MsQuic->StreamShutdown(Stream, QUIC_STREAM_SHUTDOWN_FLAG_GRACEFUL, 0);
    MsQuic->ConfigurationClose(Configuration);
    MsQuic->RegistrationClose(Registration);
    MsQuicClose(MsQuic);
    return 0;
}


