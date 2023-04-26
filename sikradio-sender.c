#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#include "err.h"

const char nienazwany[] = "Nienazwany Nadajnik";
const uint16_t port = 29289;
const int psize = 512;

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
    printf("Adres %s, port %d, packet size %d, nazwa %s\n", DEST_ADDR, DATA_PORT, PSIZE, NAZWA);

    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    ENSURE(socket_fd > 0);

    struct sockaddr_in client_address;
    client_address.sin_family = AF_INET;
    client_address.sin_addr.s_addr = inet_addr(DEST_ADDR);
    client_address.sin_port = htons(DATA_PORT);

    ssize_t gupi_test = sendto(socket_fd, "Test\n", 5, 0, (struct sockaddr *)&client_address, (socklen_t)sizeof(client_address));

    printf("%ld\n", gupi_test);

    return 0;
}