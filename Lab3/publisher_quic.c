#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <msquic.h>

#define BUF_SIZE 1024
#define DEFAULT_MSGS 10

static const QUIC_API_TABLE *MsQuic = NULL;
static HQUIC Registration = NULL;
static HQUIC Configuration = NULL;

static QUIC_STATUS QUIC_API StreamCallback(HQUIC Stream, void *Context, QUIC_STREAM_EVENT *Event) {
    (void)Context;
    switch (Event->Type) {
        case QUIC_STREAM_EVENT_SEND_COMPLETE:
            return QUIC_STATUS_SUCCESS;
        case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN:
        case QUIC_STREAM_EVENT_PEER_SEND_ABORTED:
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
    const char *server;
    uint16_t port;
    const char *topic;
    const char *pub_id;
    if (argc < 5) {
        fprintf(stderr, "[publisher_quic] Using defaults: 127.0.0.1 8080 A_vs_B P1\n");
        server = "127.0.0.1";
        port = 8080;
        topic = "A_vs_B";
        pub_id = "P1";
    } else {
        server = argv[1];
        port = (uint16_t)atoi(argv[2]);
        topic = argv[3];
        pub_id = argv[4];
    }

    if (MsQuicOpen2(&MsQuic) != QUIC_STATUS_SUCCESS) return 1;
    QUIC_REGISTRATION_CONFIG regConfig = { "publisher-quic", QUIC_EXECUTION_PROFILE_LOW_LATENCY };
    if (MsQuic->RegistrationOpen(&regConfig, &Registration) != QUIC_STATUS_SUCCESS) return 1;

    const char *alpn = "pubsub";
    QUIC_BUFFER alpnBuffer = { (uint32_t)strlen(alpn), (uint8_t *)alpn };
    QUIC_SETTINGS settings;
    memset(&settings, 0, sizeof(settings));
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

    char out[BUF_SIZE];
    for (int i = 1; i <= DEFAULT_MSGS; ++i) {
        int n = snprintf(out, sizeof(out), "PUBLISH %s [%s] message %d\n", topic, pub_id, i);
        if (n < 0) break;
        QUIC_BUFFER buf;
        buf.Length = (uint32_t)n;
        buf.Buffer = (uint8_t *)out;
        MsQuic->StreamSend(Stream, &buf, 1, QUIC_SEND_FLAG_ALLOW_0_RTT, NULL);
        sleep(1);
    }

    MsQuic->StreamShutdown(Stream, QUIC_STREAM_SHUTDOWN_FLAG_GRACEFUL, 0);
    Sleep(500);

    MsQuic->ConfigurationClose(Configuration);
    MsQuic->RegistrationClose(Registration);
    MsQuicClose(MsQuic);
    return 0;
}


