#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>

#include "err.h"

const uint16_t port = 29289;
const int64_t bsize = 65536;

struct thread_parameters
{
    int8_t *circular_buffer;
    int PSIZE;                        // to się NIE POWINNO zmienić w jednej piosence
    int64_t BSIZE;                    // to się nie zmienia więc nie musi być wskaźnikiem, cnie?
    uint64_t *first_byte_to_come_in;  // numer bajtu, który ZARAZ powinien zostać wczytany
    uint64_t *first_byte_to_come_out; // numer bajtu, który ZARAZ powinien zostać wypisany na stdout
    pthread_mutex_t *mutex;
    pthread_cond_t *cv_in;
    pthread_cond_t *cv_out;
    int8_t *stop_new_session_id; // 0 = false, 1 = true, inne wartości useless ale bool też ma 8 bitów, więc whatever
};

uint64_t ntohll(uint64_t num)
{
    uint64_t result = 0;
    uint32_t high_bits, low_bits;
    high_bits = num >> 32;
    low_bits = num & UINT32_MAX;
    result = ntohl(low_bits);
    result = result << 32;
    result += ntohl(high_bits);
    return result;
}

ssize_t circular_to_buffer_copy(int8_t *circular, int8_t *buffer, int64_t BSIZE, uint64_t beginning, uint64_t end, int PSIZE)
{
    printf("Circular to buffer copy is being made!\n");

    BSIZE /= PSIZE;
    BSIZE *= PSIZE; // lokalne udawanie że bufor ma długość podzielną przez rozmiar paczki
    if ((end <= beginning) || (beginning + BSIZE < end))
        fatal("Wrong arguments: beginning %ld, end %ld.\n", beginning, end);
    bzero(buffer, BSIZE); // WAŻNA LINIJKA ROBIĄCA DUŻO ZA NAS
    uint64_t data_to_transfer_size = end - beginning;
    uint64_t data_transferred_size = 0;
    if (beginning % BSIZE < end % BSIZE)
    {
        memcpy(buffer, circular + (beginning % BSIZE), data_to_transfer_size);
    }
    else
    {
        // mamy bufor circular w dwóch częściach, God Damn It.
        memcpy(buffer, circular + (beginning % BSIZE), BSIZE - (beginning % BSIZE));
        data_transferred_size = BSIZE - (beginning % BSIZE);
        memcpy(buffer + data_transferred_size, circular, data_to_transfer_size - data_transferred_size);
    }
    for (uint64_t i = 0; i < data_to_transfer_size; i++)
    {
        if (circular[(beginning + i) % BSIZE] != buffer[i])
        {
            fatal("Copy from circular buffer to thread buffer went wrong!\n");
        }
    }
    printf("Circular to buffer copy done!\n");

    return data_to_transfer_size;
}

void *write_to_circular(int8_t *circular, int8_t *source, int64_t BSIZE, ssize_t data_size, uint64_t circular_beginning, int PSIZE)
{
    BSIZE /= PSIZE; // lokalne zmiany
    BSIZE *= PSIZE; // będziemy udawać, że bufor ma długość podzielną przez rozmiar paczki
    printf("Write to circular occurs!\n");
    if (circular_beginning + data_size < BSIZE)
    {
        // zmieści się na raz
        memcpy(circular + circular_beginning, source, data_size);
        for (uint64_t i = 0; i < data_size; i++)
        {
            if (circular[i + circular_beginning] != source[i])
            {
                fatal("Copy from temp_buf to circular buffer went wrong (EASY version!).\n");
            }
        }
    }
    else
    {
        ssize_t data_to_transfer = BSIZE - circular_beginning;
        memcpy(circular + circular_beginning, source, data_to_transfer);
        ssize_t data_left = data_size - data_to_transfer;
        memcpy(circular, source + data_to_transfer, data_left);
        for (uint64_t i = 0; i < data_size; i++)
        {
            if (circular[(i + circular_beginning) % BSIZE] != source[i])
            {
                fatal("Copy from temp_buf to circular buffer went wrong (HARD version).\n");
            }
        }
    }
    printf("Write to circular ends!\n");
    return NULL;
}

void *thread_write_to_stdout(void *args)
{
    struct thread_parameters *param = (struct thread_parameters *)args;
    int8_t *buffer = malloc(param->BSIZE);
    ssize_t data_transferred_size;
    ssize_t write_size, total_write_size;

    printf("Hi! I'm a THREAD.\n"); // DEBUG

    char flags = 'a';
    char *flags_pointer = &flags;
    FILE *dummy = fopen("Song Copy", flags_pointer); // CRASH TESTY Z PLIKIEM

    while (true)
    {
        CHECK(pthread_mutex_lock(param->mutex));
        printf("THREAD acquired mutex!\n");
        if (*(param->stop_new_session_id) > 0)
        {
            *(param->stop_new_session_id) = 0;
            CHECK(pthread_mutex_unlock(param->mutex));
            printf("THREAD released mutex!\n");
            break;
        }
        else
        {
            // jeszcze mamy mutexa, póki mamy mutexa można kopiować bufor!
            if (*(param->first_byte_to_come_in) > *(param->first_byte_to_come_out))
            {
                data_transferred_size = circular_to_buffer_copy(param->circular_buffer, buffer, param->BSIZE, *(param->first_byte_to_come_out), *(param->first_byte_to_come_in), param->PSIZE);
                CHECK(pthread_mutex_unlock(param->mutex));
                CHECK(pthread_cond_signal(param->cv_in));
                printf("THREAD released mutex!\n");
                // write_size = write(STDOUT_FILENO, buffer, data_transferred_size); // WŁAŚCIWE
                write_size = fwrite(buffer, 1, data_transferred_size, dummy); // DUMMY
                printf("THREAD: I just wrote %ld bytes to a file, yay! (I was supposed to write %ld)\n", write_size, data_transferred_size);
                total_write_size = write_size; // reset total write size
                if (total_write_size < data_transferred_size)
                { // powinniśmy się upewnić że wypisaliśmy wszystko co się dało, ale na razie i tak NIC nie działa
                    printf("THREAD: I encountered an error I can't handle...\n");
                }
                CHECK(pthread_mutex_lock(param->mutex));
                *(param->first_byte_to_come_out) += data_transferred_size;
                CHECK(pthread_mutex_unlock(param->mutex));
            }
            else
            {
                while (*(param->first_byte_to_come_in) <= *(param->first_byte_to_come_out))
                {
                    // WAIT - robimy zmienną warunkową żeby ogarnąć wymianę muteksem ;w;
                    printf("THREAD: no new data, waiting!\n");
                    CHECK(pthread_cond_signal(param->cv_in));
                    CHECK(pthread_cond_wait(param->cv_out, param->mutex));
                }
                CHECK(pthread_mutex_unlock(param->mutex));
                printf("THREAD released mutex! (and is not going to write anything cause there's no new data)\n");
            }
        }
    }

    printf("THREAD will close now!\n");

    fclose(dummy);

    free(buffer);
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        fatal("Too few arguments, use:\n./sikradio-sender -a receive_address\noptional: -P (port), -b (buffer size)\n");
    }
    char DEST_ADDR[32];
    DEST_ADDR[0] = '\0';
    uint16_t DATA_PORT = port;
    int64_t BSIZE = bsize;
    for (int i = 1; i < argc; i += 2)
    {
        if (strcmp(argv[i], "-a") == 0)
        {
            memcpy(DEST_ADDR, argv[i + 1], strlen(argv[i + 1])); // uwaga na złe argumenty! (TODO: HANDLING)
        }
        else if (strcmp(argv[i], "-P") == 0)
        {
            DATA_PORT = atoi(argv[i + 1]);
            if (DATA_PORT == 0)
            {
                DATA_PORT = port;
            }
        }
        else if (strcmp(argv[i], "-b") == 0)
        {
            BSIZE = atoi(argv[i + 1]);
            if (BSIZE <= 0)
            {
                fatal("Buffer size below or equal to zero - no space to store data!\n");
            }
        }
    }
    printf("Adres %s, port %d, buffer size %ld\n", DEST_ADDR, DATA_PORT, BSIZE);
    int8_t *buf = malloc(BSIZE);
    int8_t *temp_buf = malloc(BSIZE); // [*] oszczędność pamięci
    bzero(buf, BSIZE);
    bzero(temp_buf, BSIZE);

    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    ENSURE(socket_fd > 0);

    struct sockaddr_in server_address;
    struct sockaddr_storage sender_address;
    char SOURCE_ADDR[32];
    socklen_t sender_address_size;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(DATA_PORT);

    CHECK_ERRNO(bind(socket_fd, (struct sockaddr *)&server_address, sizeof(server_address)));

    ssize_t read_size;
    uint64_t session_id;
    uint64_t last_session_id = 0;
    uint64_t BYTE0 = 0;
    uint64_t first_byte_num;

    // char flags = 'a';
    // char *flags_pointer = &flags;
    // FILE *dummy = fopen("Song Copy", flags_pointer); // CRASH TESTY Z PLIKIEM

    pthread_mutex_t mutex;
    CHECK(pthread_mutex_init(&mutex, NULL));
    pthread_cond_t cv_in;
    CHECK(pthread_cond_init(&cv_in, NULL));
    pthread_cond_t cv_out;
    CHECK(pthread_cond_init(&cv_out, NULL));
    int8_t stop_new_session_id = 0;
    uint64_t first_byte_to_come_in;
    uint64_t first_byte_to_come_out;
    int8_t thread_started = 0;
    struct thread_parameters param;
    pthread_t thread;

    while (true)
    {
        read_size = recvfrom(socket_fd, temp_buf, BSIZE, 0, (struct sockaddr *)&sender_address, &sender_address_size); // brak: OBSŁUGA BRAKUJĄCYCH PACZEK! (ale możemy założyć ich ten sam rozmiar, hura!)
        session_id = ntohll(((uint64_t *)temp_buf)[0]);                                                                // brak: OBSŁUGA ZMIANY NUMERU! (last_session_id)
        first_byte_num = ntohll(((uint64_t *)temp_buf)[1]);

        memcpy(SOURCE_ADDR, inet_ntoa(((struct sockaddr_in *)&sender_address)->sin_addr), 32);

        if (strcmp(SOURCE_ADDR, DEST_ADDR) == 0) // DZIAŁA - porównanie adresów IP git!
        {
            if (session_id > last_session_id) // zaczynamy odtwarzanie od nowa, jazda
            {
                last_session_id = session_id;

                CHECK(pthread_mutex_lock(&mutex));
                printf("MAIN acquired mutex!\n");
                BYTE0 = first_byte_num;
                first_byte_to_come_in = first_byte_num + read_size; // halo, w końcu już otrzymaliśmy tę paczkę!
                first_byte_to_come_out = first_byte_num;
                bzero(buf, BSIZE);
                CHECK(pthread_mutex_unlock(&mutex));

                if (thread_started)
                {
                    CHECK(pthread_mutex_lock(&mutex));
                    printf("MAIN acquired mutex!\n");
                    *(param.stop_new_session_id) = 1;
                    CHECK(pthread_mutex_unlock(&mutex));
                    pthread_join(thread, NULL);
                    thread_started = 0;
                }
            }

            CHECK(pthread_mutex_lock(&mutex));
            printf("MAIN acquired mutex!\n");
            while (first_byte_to_come_in - first_byte_to_come_out >= BSIZE)
            {
                CHECK(pthread_cond_signal(&cv_out));
                CHECK(pthread_cond_wait(&cv_in, &mutex));
            }
            write_to_circular(buf, temp_buf + (2 * sizeof(uint64_t)), BSIZE, read_size - (2 * sizeof(uint64_t)), first_byte_num % BSIZE, read_size);
            first_byte_to_come_in += read_size - (2 * sizeof(uint64_t));
            CHECK(pthread_mutex_unlock(&mutex));
            CHECK(pthread_cond_signal(&cv_out)); // signal, bo SĄ na pewno nowe dane do przesłania dalej

            // fwrite(temp_buf + (2 * sizeof(uint64_t)), read_size - (2 * sizeof(uint64_t)), 1, dummy);
            bzero(temp_buf, BSIZE);

            printf("Read %ld bytes: session id %ld, first_byte_num %ld\n", read_size, session_id, first_byte_num);
        }
        if ((first_byte_num - BYTE0 >= 3 * BSIZE / 4) && (thread_started == 0))
        {
            // LINK START
            param.BSIZE = BSIZE;
            param.PSIZE = read_size; // to zawsze powinna być po prostu wielkość paczki...
            param.circular_buffer = buf;
            first_byte_to_come_in = read_size + first_byte_num;
            param.first_byte_to_come_in = &first_byte_to_come_in;
            param.first_byte_to_come_out = &first_byte_to_come_out;
            param.stop_new_session_id = &stop_new_session_id;
            param.mutex = &mutex;
            param.cv_in = &cv_in;
            param.cv_out = &cv_out;

            pthread_create(&thread, NULL, thread_write_to_stdout, (void *)&param);
            printf("THREAD STARTED: first_byte_num - BYTE0 = %ld, 3/4 BSIZE = %ld\n", first_byte_num - BYTE0, 3 * BSIZE / 4);

            thread_started = 1;
        }
    }

    // fclose(dummy);

    close(socket_fd);

    free(buf);
    free(temp_buf);

    return 0;
}