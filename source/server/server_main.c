#include <stdio.h>
#include <sys/errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <pthread.h>
#include <poll.h>
#include <sys/un.h>

#include "request_queue.h"

#define CONFIG_PATH "./etc/"
#define CONFIG_FN "./etc/config.txt"
#define TOKEN_SYMBOL ":"
#define BUFFER_SIZE 256
#define DEFAULT_CONFIG "n_thread:4\nb_storage:100\nn_file_storage:100\nsoc_filename:null\nmax_conn_wait:10"
#define UNIX_PATH_MAX 108
#define POLL_TIMEOUT 100

struct config_struct{
    int n_thread;                                                   //Numero di thread worker da avviare all'inizio dell'esecuzione del server
    int b_storage;                                                  //Dimensione dello spazio di archiviazione in Mbyte
    int n_file_storage;                                             //Numero massimo di file che possono essere contenuti nello spazio di archiviazione
    char soc_filename[UNIX_PATH_MAX];                               //Nome del socket file
    int max_conn_wait;                                              //Numero massimo di connessioni in attesa
};

typedef struct config_struct config;

//Esegue il parsing del file di configurazione restituendo i valori di configurazione specificati
config parse_config();

//Aggiunge un nuovo socket file descriptor all'array di fd attivi
void add_fd(struct pollfd *fds, int fd);

void add_queue(struct pollfd *fds, int n, int max_conn, request_queue_el **head, request_queue_el **tail);

int main(int argc, char *argv[]){
    config config;
    int fd_socket, n_fd_socket, i;
    int terminate = 0, poll_result;
    struct sockaddr_un socket_addr;
    request_queue_el *head = NULL, *tail = NULL;
    struct pollfd *fds;

    config = parse_config();

    printf("Loaded following configuration from config.txt:\n");
    printf("Worker thread number: %d\nBuffer size: %dMbytes\nMax files number: %d\nSocket filename: %s\n", config.n_thread, config.b_storage, config.n_file_storage, config.soc_filename);

    fd_socket = socket(AF_UNIX, SOCK_STREAM, 0);

    socket_addr.sun_family = AF_UNIX;
    strncpy(socket_addr.sun_path, config.soc_filename, UNIX_PATH_MAX);

    if(bind(fd_socket,(struct sockaddr *) &socket_addr, sizeof(socket_addr))) {
        perror("Binding socket: ");

        return -1;
    }

    if(listen(fd_socket, config.max_conn_wait)) {
        perror("Listen on socket: ");

        return -1;
    }

    fds = malloc(config.max_conn_wait * sizeof(struct pollfd));

    for(i = 0; i < config.max_conn_wait; i++) {
        fds[i].fd = -1;
    }

    while(!terminate) {
        if(n_fd_socket = accept(fd_socket, NULL, NULL) == -1) {
            perror("Accepting connection on socket: ");
        } else {
            add_fd(fds, n_fd_socket);

            if(poll_result = poll(fds, config.max_conn_wait, POLL_TIMEOUT) > 0) {
                add_queue(fds, poll_result, config.max_conn_wait, &head, &tail);
            }
        }
    }

    free(fds);
}

config parse_config() {
    FILE *conf_fp;                                                  //Puntatore al file di configurazione
    char *tag_name, *value;
    config result;

    if(access(CONFIG_FN, R_OK) == -1) {
        //Verifico l'esistenza del file di configurazione
        if(errno == ENOENT) {
            //Il file non esiste
            mkdir(CONFIG_PATH,S_IRWXU);

            printf("Creating config.txt file...\n");
            conf_fp = fopen(CONFIG_FN, "w");

            //Inizializzo il file config.txt con la configurazione dei default
            printf("Initializing configuration file...\n\n");
            fwrite(DEFAULT_CONFIG, strlen(DEFAULT_CONFIG), sizeof(char), conf_fp);

            fclose(conf_fp);
        }
    }

    conf_fp = fopen(CONFIG_FN, "r");

    char *buffer = malloc(BUFFER_SIZE * sizeof(char));

    if(buffer == NULL) {
        perror("Allocating buffer for read config file:");
    }

    //Leggo il contenuto del file config.txt
    while(!feof(conf_fp)) {
        //Ottengo una riga del file
        if(fgets(buffer, BUFFER_SIZE, conf_fp) == NULL) {
            //Verifico la presenza di errori
            if(!feof(conf_fp)) {
                if(ferror(conf_fp)) {
                    perror("Reading config file:");

                    return result;
                }
            }
        } else {
            tag_name = strtok(buffer, TOKEN_SYMBOL);
            value = strtok(NULL, TOKEN_SYMBOL);

            //Imposto i campi della struct usata per la memorizzazione delle impostazioni
            if(!strcmp(tag_name, "n_thread")) {
                result.n_thread = (int)(strtol(value, NULL, 10));

            } else if(!strcmp(tag_name, "b_storage")) {
                result.b_storage = (int)(strtol(value, NULL, 10));

            } else if(!strcmp(tag_name, "n_file_storage")) {
                result.n_file_storage = (int)(strtol(value, NULL, 10));

            } else if(!strcmp(tag_name, "soc_filename")) {
                //result.soc_filename = malloc(strlen(value) * sizeof(char));
                strcpy(result.soc_filename, value);

            } else {
                printf("Parsed setting not supported, please check config.txt file: %s\n", tag_name);
            }
        }
    }

    free(buffer);

    return result;
}

void add_fd(struct pollfd *fds, int fd) {
    int i = 0;

    while(fds[i].fd != -1) {
        i++;
    }

    fds[i].fd = fd;
    fds[i].events = POLLIN;
}

void add_queue(struct pollfd *fds, int n, int maxconn, request_queue_el **head, request_queue_el **tail) {
    int find = 0, i = 0;

    while(find < n && i < maxconn) {
        if(fds[i].revents == POLLIN) {
            *tail = push_request(head, *tail, fds[i].fd);
        }
    }
}
