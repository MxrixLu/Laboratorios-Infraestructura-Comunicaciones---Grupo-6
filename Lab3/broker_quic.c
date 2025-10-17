#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <msquic.h>

#define MAX_CLIENTS 64
#define MAX_TOPICS 64
#define BUFFER_SIZE 2048

typedef struct {
    char topic[64];
    HQUIC subscribers[MAX_CLIENTS];
    int num_subscribers;
} Topic;

static const QUIC_API_TABLE *MsQuic = NULL;
static HQUIC Registration = NULL;
static HQUIC Configuration = NULL;
static HQUIC Listener = NULL;

static Topic topics[MAX_TOPICS];
static int num_topics = 0;

static void subscribe_to_topic(const char *topic, HQUIC stream) {
    for (int i = 0; i < num_topics; i++) {
        if (strcmp(topics[i].topic, topic) == 0) {
            if (topics[i].num_subscribers < MAX_CLIENTS) {
                topics[i].subscribers[topics[i].num_subscribers++] = stream;
            }
            return;
        }
    }
    if (num_topics < MAX_TOPICS) {
        memset(&topics[num_topics], 0, sizeof(Topic));
        strncpy(topics[num_topics].topic, topic, sizeof(topics[num_topics].topic) - 1);
        topics[num_topics].subscribers[0] = stream;
        topics[num_topics].num_subscribers = 1;
        num_topics++;
    }
}

static void publish_to_topic(const char *topic, const char *message) {
    for (int i = 0; i < num_topics; i++) {
        if (strcmp(topics[i].topic, topic) == 0) {
            for (int j = 0; j < topics[i].num_subscribers; j++) {
                HQUIC s = topics[i].subscribers[j];
                if (s != NULL) {
                    QUIC_BUFFER buf;
                    buf.Length = (uint32_t)strlen(message);
                    buf.Buffer = (uint8_t *)message;
                    MsQuic->StreamSend(s, &buf, 1, 0, NULL);
                }
            }
            return;
        }
    }
}

typedef struct {
    char buffer[BUFFER_SIZE];
    size_t length;
} StreamCtx;

static QUIC_STATUS QUIC_API StreamCallback(HQUIC Stream, void *Context, QUIC_STREAM_EVENT *Event) {
    (void)Context;
    switch (Event->Type) {
        case QUIC_STREAM_EVENT_RECEIVE: {
            StreamCtx *ctx = (StreamCtx *)MsQuic->GetContext(Stream);
            if (!ctx) {
                ctx = (StreamCtx *)calloc(1, sizeof(StreamCtx));
                MsQuic->SetContext(Stream, ctx);
            }
            for (uint32_t i = 0; i < Event->RECEIVE.BufferCount; i++) {
                QUIC_BUFFER *b = &Event->RECEIVE.Buffers[i];
                size_t copy = b->Length;
                if (ctx->length + copy >= sizeof(ctx->buffer)) copy = sizeof(ctx->buffer) - ctx->length - 1;
                memcpy(ctx->buffer + ctx->length, b->Buffer, copy);
                ctx->length += copy;
                ctx->buffer[ctx->length] = '\0';
                char *newline;
                while ((newline = strchr(ctx->buffer, '\n')) != NULL) {
                    *newline = '\0';
                    char command[16] = {0};
                    char topic[64] = {0};
                    char content[BUFFER_SIZE] = {0};
                    sscanf(ctx->buffer, "%15s %63s %[^\n]", command, topic, content);
                    if (strcmp(command, "SUBSCRIBE") == 0) {
                        subscribe_to_topic(topic, Stream);
                    } else if (strcmp(command, "PUBLISH") == 0) {
                        publish_to_topic(topic, content);
                    }
                    size_t remaining = ctx->length - (size_t)(newline - ctx->buffer + 1);
                    memmove(ctx->buffer, newline + 1, remaining);
                    ctx->length = remaining;
                    ctx->buffer[ctx->length] = '\0';
                }
            }
            return QUIC_STATUS_SUCCESS;
        }
        case QUIC_STREAM_EVENT_PEER_SEND_ABORTED:
        case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN:
        case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE: {
            StreamCtx *ctx = (StreamCtx *)MsQuic->GetContext(Stream);
            if (ctx) free(ctx);
            MsQuic->SetContext(Stream, NULL);
            MsQuic->StreamClose(Stream);
            return QUIC_STATUS_SUCCESS;
        }
        default:
            return QUIC_STATUS_SUCCESS;
    }
}

static QUIC_STATUS QUIC_API ConnectionCallback(HQUIC Connection, void *Context, QUIC_CONNECTION_EVENT *Event) {
    (void)Context;
    switch (Event->Type) {
        case QUIC_CONNECTION_EVENT_CONNECTED:
            return QUIC_STATUS_SUCCESS;
        case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED:
            MsQuic->SetCallbackHandler(Event->PEER_STREAM_STARTED.Stream, (void *)StreamCallback, NULL);
            return QUIC_STATUS_SUCCESS;
        case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
            MsQuic->ConnectionClose(Connection);
            return QUIC_STATUS_SUCCESS;
        default:
            return QUIC_STATUS_SUCCESS;
    }
}

static QUIC_STATUS QUIC_API ListenerCallback(HQUIC ListenerHandle, void *Context, QUIC_LISTENER_EVENT *Event) {
    (void)ListenerHandle;
    (void)Context;
    switch (Event->Type) {
        case QUIC_LISTENER_EVENT_NEW_CONNECTION:
            MsQuic->SetCallbackHandler(Event->NEW_CONNECTION.Connection, (void *)ConnectionCallback, NULL);
            MsQuic->ConnectionSetConfiguration(Event->NEW_CONNECTION.Connection, Configuration);
            return QUIC_STATUS_SUCCESS;
        default:
            return QUIC_STATUS_SUCCESS;
    }
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <bind_ip> <port>\n", argv[0]);
        return 1;
    }
    const char *bind_ip = argv[1];
    uint16_t port = (uint16_t)atoi(argv[2]);

    if (MsQuicOpen(&MsQuic) != QUIC_STATUS_SUCCESS) return 1;
    QUIC_REGISTRATION_CONFIG regConfig = { "broker-quic", QUIC_EXECUTION_PROFILE_LOW_LATENCY };
    if (MsQuic->RegistrationOpen(&regConfig, &Registration) != QUIC_STATUS_SUCCESS) return 1;

    const char *alpn = "pubsub";
    QUIC_BUFFER alpnBuffer = { (uint32_t)strlen(alpn), (uint8_t *)alpn };
    QUIC_SETTINGS settings;
    memset(&settings, 0, sizeof(settings));
    settings.IsSet.ServerResumptionLevel = TRUE;
    settings.ServerResumptionLevel = QUIC_SERVER_RESUME_AND_ZERORTT;

    if (MsQuic->ConfigurationOpen(Registration, &alpnBuffer, 1, &settings, sizeof(settings), NULL, &Configuration) != QUIC_STATUS_SUCCESS) return 1;
    if (MsQuic->ConfigurationLoadCredential(Configuration, &(QUIC_CREDENTIAL_CONFIG){ .Type = QUIC_CREDENTIAL_TYPE_CERTIFICATE_HASH, .Flags = QUIC_CREDENTIAL_FLAG_NONE }) != QUIC_STATUS_SUCCESS) return 1;

    if (MsQuic->ListenerOpen(Registration, ListenerCallback, NULL, &Listener) != QUIC_STATUS_SUCCESS) return 1;

    QUIC_ADDR addr;
    QuicAddrSetFamily(&addr, QUIC_ADDRESS_FAMILY_INET);
    QuicAddrSetPort(&addr, port);
    if (strcmp(bind_ip, "0.0.0.0") == 0) {
        QuicAddrSetWildcard(&addr, QUIC_ADDRESS_FAMILY_INET);
    } else {
        QuicAddrSetAddress(&addr, inet_addr(bind_ip));
    }

    QUIC_BUFFER alpnBuffer2 = { (uint32_t)strlen(alpn), (uint8_t *)alpn };
    if (MsQuic->ListenerStart(Listener, &alpnBuffer2, 1, &addr) != QUIC_STATUS_SUCCESS) return 1;
    printf("Broker QUIC listening on %s:%u\n", bind_ip, (unsigned)port);
    getchar();

    MsQuic->ListenerClose(Listener);
    MsQuic->ConfigurationClose(Configuration);
    MsQuic->RegistrationClose(Registration);
    MsQuicClose(MsQuic);
    return 0;
}


