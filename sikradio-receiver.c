#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#include "err.h"

const uint16_t port = 29289;
const int bsize = 65536;

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        fatal("Too few arguments, use:\n./sikradio-sender -a receive_address\noptional: -P (port), -b (buffer size)\n");
    }
    char DEST_ADDR[32];
    DEST_ADDR[0] = '\0';
    uint16_t DATA_PORT = port;
    int BSIZE = bsize;
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
        else if (strcmp(argv[i], "-b") == 0)
        {
            BSIZE = atoi(argv[i + 1]);
        }
    }
    printf("Adres %s, port %d, buffer size %d\n", DEST_ADDR, DATA_PORT, BSIZE);
    char *buf = malloc(BSIZE);
    bzero(buf, BSIZE);

    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    ENSURE(socket_fd > 0);

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr(DEST_ADDR);
    server_address.sin_port = htons(DATA_PORT);

    CHECK_ERRNO(bind(socket_fd, (struct sockaddr *)&server_address, sizeof(server_address)));

    ssize_t read_size = recv(socket_fd, buf, BSIZE, 0);

    printf("Read %ld bytes: %s", read_size, buf);

    free(buf);

    return 0;
}