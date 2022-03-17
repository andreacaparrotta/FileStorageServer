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
#include <signal.h>

#include "request_queue.h"
#include "resolved_queue.h"
#include "worker.h"

#define CONFIG_PATH "./etc/"
#define CONFIG_FN "./etc/config.txt"
#define TOKEN_SYMBOL ":"
#define BUFFER_SIZE 256
#define DEFAULT_CONFIG "n_thread:4\nb_storage:100\nn_file_storage:100\nsoc_filename:./server_socket\nmax_conn_wait:10"
#define UNIX_PATH_MAX 108
#define POLL_TIMEOUT 1000


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

struct pollfd *init_fds(struct pollfd *fds, int max);

void add_fd(struct pollfd *fds, int fd, int max);

int close_conn(struct pollfd *fds, int fd, int max);

void add_queue(struct pollfd *fds, int n, int maxconn, request_queue_el **head, request_queue_el **tail);

int main(int argc, char *argv[]){
    config config;

    int fd_socket, n_fd_socket;

    int terminate = 0, poll_result, i;
    int active_conn = 0;

    struct sockaddr_un socket_addr;
    struct pollfd *fds;

    request_queue_el *head_request = NULL, *tail_request = NULL;
    resolved_queue_el *head_resolved = NULL, *tail_resolved = NULL;

    pthread_t *workers;

    worker_arg *args;

    resolved_queue_el resolved;

    config = parse_config();

    printf("Loaded following configuration from config.txt:\n");
    printf("Worker thread number: %d\nBuffer size: %dMbytes\nMax files number: %d\nSocket filename: %s\nMax connection limit: %d\n\n", config.n_thread, config.b_storage, config.n_file_storage, config.soc_filename, config.max_conn_wait);

    /*
     * Crea un socket non bloccante
     */
    fd_socket = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);

    socket_addr.sun_family = AF_UNIX;
    strncpy(socket_addr.sun_path, config.soc_filename, UNIX_PATH_MAX);

    if(bind(fd_socket,(struct sockaddr *) &socket_addr, sizeof(socket_addr))) {
        perror("MANAGER: Binding socket");

        return -1;
    }

    if(listen(fd_socket, config.max_conn_wait)) {
        perror("MANAGER: Listen on socket");

        return -1;
    }

    printf("MANAGER: Socket created\n");

    /*
     * Alloca e inizializza l'array che contiene i file descriptor delle connessioni aperte
     */
    fds = malloc(config.max_conn_wait * sizeof(struct pollfd));
    fds = init_fds(fds, config.max_conn_wait);

    /*
     * Alloca l'array per contenere i thread worker presenti nel thread pool
     */
    workers = malloc(config.n_thread * sizeof(pthread_t));

    args = malloc(sizeof(worker_arg));

    args->head_request = &head_request;
    args->head_resolved = &head_resolved;
    args->tail_resolved = &tail_resolved;

    /*
     * Crea e avvia i thread worker del thread pool
     */
    for(i = 0; i < config.n_thread; i++) {
        args->thread_n = i;
        
        if((errno = pthread_create(&(workers[i]), NULL, &main_worker, args)) != 0) {
            perror("MANAGER: Creating worker threads");
        }
    }

    sleep(1);

    free(args);

    printf("MANAGER: Thread pool created\n");

    while(!terminate) {
        /*
         * Accetta una nuova connessione
         */
        n_fd_socket = accept(fd_socket, NULL, 0);

        /*
         * Verifica se la richiesta di connessione è stata accettata
         */
        if(n_fd_socket == -1) {
            if(errno != EAGAIN) {
                perror("MANAGER: Accepting connection on socket");
            }
        } else {
            printf("MANAGER: Accepted new request\n");

            add_fd(fds, n_fd_socket, config.max_conn_wait);

            active_conn++;
        }

        /*
         * Verifica se qualche connessione è pronta per poter essere letta
         */
        poll_result = poll(fds, active_conn, POLL_TIMEOUT);

        if(poll_result == -1) {
            perror("MANAGER: Polling");
        }

        if(poll_result > 0) {
            /*
             * Aggiunge eventuali nuove richieste nella coda delle richieste
             */
            add_queue(fds, poll_result, config.max_conn_wait, &head_request, &tail_request);
        }

        errno = 0;
        resolved = pop_resolved(&head_resolved);
        while(resolved.resolved_fd != -1) {
            if(resolved.close) {
                close_conn(fds, resolved.resolved_fd, config.max_conn_wait);

                active_conn--;
            } else {
                add_fd(fds, resolved.resolved_fd, config.max_conn_wait);
            }

            resolved = pop_resolved(&head_resolved);
        }

        if(resolved.resolved_fd == -1 && errno != 0) {
            perror("MANAGER: Reading resolved queue");
        }
    }

    close(fd_socket);

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
                strcpy(result.soc_filename, value);
                result.soc_filename[strcspn(result.soc_filename, "\n")] = 0;
            } else if(!strcmp(tag_name, "max_conn_wait")) {
                result.max_conn_wait = (int)(strtol(value, NULL, 10));

            } else {
                printf("Parsed setting not supported, please check config.txt file: %s\n", tag_name);
            }
        }
    }

    free(buffer);

    return result;
}

struct pollfd *init_fds(struct pollfd *fds, int max) {
    int i;

    for(i = 0; i < max; i++) {
        fds[i].fd = -1;
    }

    return fds;
}

void add_fd(struct pollfd *fds, int fd, int max) {
    int i = 0, find = 0;

    while(i < max && fds[i].fd != fd) {
        i++;
    }

    if(fds[i].fd == fd) {
        fds[i].events = POLLIN;

        find = 1;
    }

    if(!find) {
        i = 0;

        while(i < max && fds[i].fd != -1) {
            i++;
        }

        fds[i].fd = fd;
        fds[i].events = POLLIN;
    }
}

int close_conn(struct pollfd *fds, int fd, int max) {
    int i = 0;

    while(i < max && fds[i].fd != fd) {
        i++;
    }

    if(fds[i].fd == fd) {
        fds[i].fd = -1;
        fds[i].events = 0;

        return 0;
    }

    return -1;
}

void add_queue(struct pollfd *fds, int n, int maxconn, request_queue_el **head, request_queue_el **tail) {
    int find = 0, i = 0;
    request_queue_el *push_result;

    while(find < n && i < maxconn) {

        if((fds[i].events & POLLIN) && (fds[i].revents & POLLIN) && !exist_fd(*head, fds[i].fd)) {

            if((push_result = push_request(head, tail, fds[i].fd)) == NULL) {
                perror("MANAGER: Pushing new request");
            }

            fds[i].events = POLLOUT;
            fds[i].revents = 0;

            find++;
        }

        i++;
    }
}
