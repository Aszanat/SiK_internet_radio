#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>

#include "err.h"

uint64_t htonll(uint64_t num)
{
    uint64_t result = 0;
    uint32_t high_bits, low_bits;
    high_bits = num >> 32;
    low_bits = num & UINT32_MAX;
    result = htonl(low_bits);
    result = result << 32;
    result += htonl(high_bits);
    return result;
}

const char nienazwany[] = "Nienazwany Nadajnik";
const uint16_t port = 29289;
const int psize = 512;

int song_sender(int PSIZE, int socket_fd, struct sockaddr_in *client_address_pointer)
{
    time_t session_id_int = time(NULL);
    uint64_t session_id = htonll(session_id_int);
    uint64_t first_byte_num = 0;
    uint64_t first_byte_num_ordered;
    ssize_t bytes_read;
    ssize_t packet_current_size = 0;
    ssize_t write_size;
    int8_t *packet = malloc(PSIZE + 2 * sizeof(uint64_t));

    while (true)
    {
        first_byte_num_ordered = htonll(first_byte_num);

        bytes_read = read(STDIN_FILENO, packet + packet_current_size + 2 * sizeof(uint64_t), PSIZE - packet_current_size);
        if (bytes_read == 0)
        {
            printf("I ate the whole song!\n");
            break;
        }
        packet_current_size += bytes_read;
        if (packet_current_size == (ssize_t)PSIZE) // WYSYŁAMY - audio_data ma dokładnie PSIZE bajtów...
        {
            packet_current_size = 0;
            printf("I ate another %d bytes of the song! (session id PRAWDZIWE: %ld, first_byte_num: %ld) \n", PSIZE, session_id_int, first_byte_num);
            memcpy((uint64_t *)packet, &session_id, sizeof(uint64_t));
            memcpy((uint64_t *)(packet + sizeof(uint64_t)), &first_byte_num_ordered, sizeof(uint64_t));
            write_size = sendto(socket_fd, packet, PSIZE, 0, (struct sockaddr *)client_address_pointer, (socklen_t)sizeof(*client_address_pointer));
            if (write_size < PSIZE)
            {
                fatal("Halo, cos nie tak?\n");
            }
            bzero(packet, PSIZE);
            first_byte_num += PSIZE;
        }
    }

    free(packet);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        fatal("Too few arguments, use:\n./sikradio-sender -a receive_address\noptional: -P (port), -p (packet size), -n (name)\n");
    }
    char DEST_ADDR[32];
    DEST_ADDR[0] = '\0';
    uint16_t DATA_PORT = port;
    int PSIZE = psize;
    char NAZWA[512];
    memcpy(NAZWA, nienazwany, sizeof(nienazwany));

    for (int i = 1; i < argc; i += 2)
    {
        if (strcmp(argv[i], "-a") == 0)
        {
            memcpy(DEST_ADDR, argv[i + 1], strlen(argv[i + 1]));
        }
        else if (strcmp(argv[i], "-P") == 0)
        {
            DATA_PORT = atoi(argv[i + 1]);
        }
        else if (strcmp(argv[i], "-p") == 0)
        {
            PSIZE = atoi(argv[i + 1]);
        }
        else if (strcmp(argv[i], "-n") == 0)
        {
            int len = strlen(argv[i + 1]);
            memcpy(NAZWA, argv[i + 1], len);
            NAZWA[len] = '\0';
        }
    }

    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    ENSURE(socket_fd > 0);

    struct sockaddr_in client_address;
    client_address.sin_family = AF_INET;
    client_address.sin_addr.s_addr = inet_addr(DEST_ADDR);
    client_address.sin_port = htons(DATA_PORT);

    // ODTĄD ZAJMUJE SIĘ TYM FUNKCJA

    CHECK(song_sender(PSIZE, socket_fd, &client_address));

    return 0;
}