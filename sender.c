#include <arpa/inet.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

enum {
    SOURCE_PORT = 47010,
    RELAY_PORT = 47001,
    PAYLOAD_BYTES = 160,
    HARNESS_HEADER = 4,
    WIRE_HEADER = 1,
    WIRE_BYTES = WIRE_HEADER + PAYLOAD_BYTES,
    TYPE_DATA = 0,
    TYPE_ADJ = 1,
    TYPE_DIAG = 2,
    MAX_FRAMES = 65536
};

static unsigned char history[MAX_FRAMES][PAYLOAD_BYTES];

static uint32_t get_u32(const unsigned char *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void send_wire(int fd, const struct sockaddr_in *dst, unsigned char type,
                      uint16_t seq, const unsigned char payload[PAYLOAD_BYTES]) {
    unsigned char pkt[WIRE_BYTES];
    pkt[0] = (unsigned char)(((type & 0x03u) << 6) | (seq & 0x3f));
    memcpy(pkt + WIRE_HEADER, payload, PAYLOAD_BYTES);
    sendto(fd, pkt, sizeof pkt, 0, (const struct sockaddr *)dst, sizeof *dst);
}

int main(void) {
    int in_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (in_fd < 0) {
        perror("socket input");
        return 1;
    }

    struct sockaddr_in in_addr;
    memset(&in_addr, 0, sizeof in_addr);
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(SOURCE_PORT);
    in_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (bind(in_fd, (struct sockaddr *)&in_addr, sizeof in_addr) < 0) {
        perror("bind 47010");
        return 1;
    }

    int out_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (out_fd < 0) {
        perror("socket output");
        return 1;
    }

    struct sockaddr_in relay;
    memset(&relay, 0, sizeof relay);
    relay.sin_family = AF_INET;
    relay.sin_port = htons(RELAY_PORT);
    relay.sin_addr.s_addr = inet_addr("127.0.0.1");

    unsigned char buf[HARNESS_HEADER + PAYLOAD_BYTES];
    uint16_t prev_seq = 0;
    int have_prev_even = 0;

    for (;;) {
        ssize_t n = recvfrom(in_fd, buf, sizeof buf, 0, NULL, NULL);
        if (n != (ssize_t)sizeof buf) {
            if (n < 0 && errno != EINTR) perror("recvfrom source");
            continue;
        }

        uint32_t seq32 = get_u32(buf);
        uint16_t seq = (uint16_t)(seq32 & 0xffff);
        const unsigned char *payload = buf + HARNESS_HEADER;
        memcpy(history[seq], payload, PAYLOAD_BYTES);

        send_wire(out_fd, &relay, TYPE_DATA, seq, payload);

        if ((seq & 1u) == 0) {
            prev_seq = seq;
            have_prev_even = 1;
        } else if (have_prev_even && (uint16_t)(prev_seq + 1u) == seq) {
            unsigned char parity[PAYLOAD_BYTES];
            for (int i = 0; i < PAYLOAD_BYTES; i++) {
                parity[i] = (unsigned char)(history[prev_seq][i] ^ payload[i]);
            }
            send_wire(out_fd, &relay, TYPE_ADJ, prev_seq, parity);

            if (seq >= 3 && ((prev_seq >> 1) % 32u) != 0u) {
                uint16_t old_seq = (uint16_t)(seq - 3u);
                for (int i = 0; i < PAYLOAD_BYTES; i++) {
                    parity[i] = (unsigned char)(history[old_seq][i] ^ payload[i]);
                }
                send_wire(out_fd, &relay, TYPE_DIAG, seq, parity);
            }
            have_prev_even = 0;
        }
    }
}
