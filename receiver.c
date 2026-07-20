#include <arpa/inet.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

enum {
    RELAY_PORT = 47002,
    PLAYER_PORT = 47020,
    PAYLOAD_BYTES = 160,
    WIRE_HEADER = 1,
    WIRE_BYTES = WIRE_HEADER + PAYLOAD_BYTES,
    OUT_BYTES = 4 + PAYLOAD_BYTES,
    TYPE_DATA = 0,
    TYPE_ADJ = 1,
    TYPE_DIAG = 2,
    MAX_FRAMES = 65536,
    MAX_PAIRS = 32768
};

struct frame_slot {
    unsigned char payload[PAYLOAD_BYTES];
    unsigned char have;
    unsigned char sent;
};

struct parity_slot {
    unsigned char payload[PAYLOAD_BYTES];
    uint16_t a;
    uint16_t b;
    unsigned char have;
};

static struct frame_slot frames[MAX_FRAMES];
static struct parity_slot adj[MAX_PAIRS];
static struct parity_slot diag[MAX_PAIRS];

static void put_u32(unsigned char *p, uint32_t v) {
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)((v >> 16) & 0xff);
    p[2] = (unsigned char)((v >> 8) & 0xff);
    p[3] = (unsigned char)(v & 0xff);
}

static double now_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

static uint16_t expand_seq(unsigned int low6, double t0) {
    double frames_now = (now_s() - t0) * 50.0;
    int estimate = (int)(frames_now + 0.5);
    int base = estimate - ((estimate - (int)low6) & 63);
    int best = base;
    int best_abs = best > estimate ? best - estimate : estimate - best;

    int c = base + 64;
    int d = c > estimate ? c - estimate : estimate - c;
    if (d < best_abs) {
        best = c;
        best_abs = d;
    }
    c = base - 64;
    d = c > estimate ? c - estimate : estimate - c;
    if (d < best_abs) best = c;
    if (best < 0) best = (int)low6;
    return (uint16_t)best;
}

static void send_frame(int fd, const struct sockaddr_in *player, uint16_t seq) {
    struct frame_slot *fr = &frames[seq];
    if (!fr->have || fr->sent) return;

    unsigned char out[OUT_BYTES];
    put_u32(out, (uint32_t)seq);
    memcpy(out + 4, fr->payload, PAYLOAD_BYTES);
    sendto(fd, out, sizeof out, 0, (const struct sockaddr *)player,
           sizeof *player);
    fr->sent = 1;
}

static int try_repair_edge(struct parity_slot *pa) {
    struct frame_slot *a = &frames[pa->a];
    struct frame_slot *b = &frames[pa->b];

    if (!pa->have) return 0;
    if (a->have && !b->have) {
        for (int i = 0; i < PAYLOAD_BYTES; i++) {
            b->payload[i] = (unsigned char)(pa->payload[i] ^ a->payload[i]);
        }
        b->have = 1;
        return 1;
    } else if (!a->have && b->have) {
        for (int i = 0; i < PAYLOAD_BYTES; i++) {
            a->payload[i] = (unsigned char)(pa->payload[i] ^ b->payload[i]);
        }
        a->have = 1;
        return 1;
    }
    return 0;
}

static void try_repair_near(uint16_t seq) {
    for (int pass = 0; pass < 4; pass++) {
        int changed = 0;
        for (int d = -8; d <= 8; d++) {
            int s = (int)seq + d;
            if (s < 0 || s >= MAX_FRAMES) continue;
            uint16_t u = (uint16_t)s;
            changed |= try_repair_edge(&adj[u >> 1]);
            if (u & 1u) changed |= try_repair_edge(&diag[u >> 1]);
            if (u >= 3 && ((u + 3u) & 1u)) {
                changed |= try_repair_edge(&diag[(u + 3u) >> 1]);
            }
        }
        if (!changed) break;
    }
}

static void send_near(int fd, const struct sockaddr_in *player, uint16_t seq) {
    for (int d = -8; d <= 8; d++) {
        int s = (int)seq + d;
        if (s >= 0 && s < MAX_FRAMES) send_frame(fd, player, (uint16_t)s);
    }
}

int main(void) {
    const char *t0_env = getenv("T0");
    double t0 = t0_env ? atof(t0_env) : now_s();

    int in_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (in_fd < 0) {
        perror("socket input");
        return 1;
    }

    struct sockaddr_in in_addr;
    memset(&in_addr, 0, sizeof in_addr);
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(RELAY_PORT);
    in_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (bind(in_fd, (struct sockaddr *)&in_addr, sizeof in_addr) < 0) {
        perror("bind 47002");
        return 1;
    }

    int out_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (out_fd < 0) {
        perror("socket output");
        return 1;
    }

    struct sockaddr_in player;
    memset(&player, 0, sizeof player);
    player.sin_family = AF_INET;
    player.sin_port = htons(PLAYER_PORT);
    player.sin_addr.s_addr = inet_addr("127.0.0.1");

    unsigned char pkt[2048];
    for (;;) {
        ssize_t n = recvfrom(in_fd, pkt, sizeof pkt, 0, NULL, NULL);
        if (n != WIRE_BYTES) {
            if (n < 0 && errno != EINTR) perror("recvfrom relay");
            continue;
        }

        unsigned char type = (unsigned char)((pkt[0] >> 6) & 0x03u);
        uint16_t seq = expand_seq((unsigned int)(pkt[0] & 0x3fu), t0);
        unsigned char *payload = pkt + WIRE_HEADER;

        if (type == TYPE_DATA) {
            struct frame_slot *fr = &frames[seq];
            if (!fr->have) {
                memcpy(fr->payload, payload, PAYLOAD_BYTES);
                fr->have = 1;
            }
            try_repair_near(seq);
            send_near(out_fd, &player, seq);
        } else if (type == TYPE_ADJ) {
            uint16_t even_seq = (uint16_t)(seq & 0xfffeu);
            struct parity_slot *pa = &adj[even_seq >> 1];
            if (!pa->have) {
                memcpy(pa->payload, payload, PAYLOAD_BYTES);
                pa->a = even_seq;
                pa->b = (uint16_t)(even_seq + 1u);
                pa->have = 1;
            }
            try_repair_near(even_seq);
            send_near(out_fd, &player, even_seq);
        } else if (type == TYPE_DIAG && seq >= 3) {
            struct parity_slot *pa = &diag[seq >> 1];
            if (!pa->have) {
                memcpy(pa->payload, payload, PAYLOAD_BYTES);
                pa->a = (uint16_t)(seq - 3u);
                pa->b = seq;
                pa->have = 1;
            }
            try_repair_near(seq);
            send_near(out_fd, &player, seq);
        }
    }
}
