#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>
#include <pthread.h>

#define SERVER_PORT 8080  /* arbitrary, but client & server must agree */
#define BUF_SIZE 4096  /* block transfer size */
#define QUEUE_SIZE 10

struct id_time_table {
    unsigned int ID;
    time_t seconds;
};

// Tabela global para armazenar IDs e timestamps
struct id_time_table table[4096];
pthread_mutex_t table_lock; // Mutex para proteger o acesso à tabela

void *handle_client(void *arg) {
    int sa = *(int *)arg;
    free(arg);

    unsigned char buf[BUF_SIZE]; /* buffer for incoming request */
    unsigned int last_access;
    int flag_id_zero = 0;

    read(sa, buf, BUF_SIZE); /* read file name from socket */

    /* Request type is in buf[0] */
    int type = buf[0];

    /* Request ID is in buf[1] through buf[4] */
    unsigned int id = buf[4] << 24 | buf[3] << 16 | buf[2] << 8 | buf[1];

    if (id == 0) {
        flag_id_zero = 1;
        unsigned int new_id[4];
        new_id[0] = rand() % 256;
        new_id[1] = rand() % 256;
        new_id[2] = rand() % 256;
        new_id[3] = rand() % 256;
        id = new_id[0] << 24 | new_id[1] << 16 | new_id[2] << 8 | new_id[3];
    }

    pthread_mutex_lock(&table_lock); // Bloqueia a tabela para acesso seguro
    for (int i = 0; i < 4096; i++) {
        if (table[i].ID == id) {
            last_access = table[i].seconds;
            table[i].seconds = time(NULL);
            break;
        } else if (table[i].ID == 0) {
            table[i].ID = id;
            table[i].seconds = time(NULL);
            break;
        }
    }
    pthread_mutex_unlock(&table_lock); // Desbloqueia a tabela

    if (type == 1) {
        /* Request file name is in buf[5] through buf[BUF_SIZE] */
        int aux;
        char filename[BUF_SIZE];
        unsigned char answer[BUF_SIZE];
        answer[0] = 2;
        memcpy(&answer[1], &id, 4);

        for (aux = 5; aux < BUF_SIZE; aux++) {
            filename[aux - 5] = buf[aux];
            if (buf[aux] == 0) break;
        }
        filename[aux - 5] = '\0';

        printf("type = %u, id = %u, filename = %s\n", type, id, filename);

        /* Print the request in hex format. */
        for (int b = 0; b < aux; b++) {
            printf("%02x ", (unsigned char)buf[b]);
        }
        printf("\n");

        /* Get and return the file. */
        int fd = open(filename, O_RDONLY); /* open the file to be sent back */
        if (fd < 0) printf("open failed");
        bool first_loop = true;
        bool end = false;
        while (!end) {
            int bytes = read(fd, &answer[5], BUF_SIZE - 5); /* read from file */
            if (bytes <= 0 &&  first_loop){
               answer[0] = 5;
               end = true;
            }
            else if(bytes <= 0){
               break;
            }  /* check for end of file */
            write(sa, answer, bytes + 5);  /* write bytes to socket */
            printf("wrote %u bytes\n", bytes);
            first_loop = false;
        }
        close(fd); /* close file */
    } else if (type == 3) {
        printf("type = %u, id = %u\n", type, id);
        // MOCK - write the answer as type 4 and the same id and a random number
        unsigned char answer[BUF_SIZE];

        memcpy(&answer[1], &id, 4);
        char numeroString[12];
        if (flag_id_zero == 0) {
            answer[0] = 4;
            sprintf(numeroString, "%u", last_access);
            memcpy(&answer[5], &numeroString, 12);
            write(sa, answer, 16);
            printf("wrote %u bytes\n", 16);

        } else {
            answer[0] = 6;
            unsigned char BUF[] = "NULL";
            memcpy(&answer[5], &BUF, 4);
            write(sa, answer, 9);
            printf("wrote %u bytes\n", 9);
        }
    }
    close(sa); /* close connection */
    return NULL;
}

int main(int argc, char *argv[]) {
    int s, b, l, on = 1;
    struct sockaddr_in channel; /* holds IP address */

    /* Initialize the table */
    memset(table, 0, sizeof(table));
    pthread_mutex_init(&table_lock, NULL); // Initialize mutex

    /* Build address structure to bind to socket. */
    memset(&channel, 0, sizeof(channel)); /* zero channel */
    channel.sin_family = AF_INET;
    channel.sin_addr.s_addr = htonl(INADDR_ANY);
    channel.sin_port = htons(SERVER_PORT);

    /* Passive open. Wait for connection. */
    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); /* create socket */
    if (s < 0) {
        printf("socket call failed");
        exit(-1);
    }
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *) &on, sizeof(on));

    b = bind(s, (struct sockaddr *) &channel, sizeof(channel));
    if (b < 0) {
        printf("bind failed");
        exit(-1);
    }

    l = listen(s, QUEUE_SIZE); /* specify queue size */
    if (l < 0) {
        printf("listen failed");
        exit(-1);
    }

    /* Socket is now set up and bound. Wait for connection and process it. */
    while (1) {
        int *sa = (int *)malloc(sizeof(int)); // Conversão explícita aqui
        if (!sa) {
            printf("malloc failed");
            continue;
        }

        *sa = accept(s, 0, 0); /* block for connection request */
        if (*sa < 0) {
            printf("accept failed");
            free(sa);
            continue;
        }

        pthread_t thread;
        pthread_create(&thread, NULL, handle_client, sa);
        pthread_detach(thread); // Detach the thread to free resources
    }

    pthread_mutex_destroy(&table_lock); // Destroy mutex
    close(s); /* close server socket */
    return 0;
}
