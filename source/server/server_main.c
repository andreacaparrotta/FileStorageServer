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
#include "storage_manager.h"
#include "worker.h"

#define CONFIG_PATH "./etc/"
#define CONFIG_FN "./etc/config.txt"
#define TOKEN_SYMBOL ":"                        // Simbolo separatore nel file gi configurazione
#define BUFFER_SIZE 256                         // Dimensione del buffer usato per la lettura del file di configurazione
#define DEFAULT_CONFIG "# Il numero di thread che compongono il thread pool\nn_thread:1\n# La dimensione massima dello storage espressa in Mbyte\nb_storage:128\n# Il numero massimo di file che possono essere presenti contemporaneamente nello storage\nn_file_storage:10000\n# Il filename del socket di ascolto del server\nsoc_filename:./etc/server_socket\n# Il numero massimo di connessioni in attesa di essere accettate\nmax_conn_wait:10\n# Il numero massimo di connessioni attive contemporaneamente\nmax_active_conn:10\n# Il timeout di attesa del server\nmanager_timeout:10\n# Il file name del file di log\nlog_filename:./etc/log.txt\n# Il timeout per chiudere le connessioni inutilizzate con i client, specificato in secondi\nclient_timeout:60"
#define UNIX_PATH_MAX 108
#define CLIENT_TIMEOUT 60

struct config_struct{
    int n_thread;                                                   // Numero di thread worker che compongono il thread pool
    double b_storage;                                               // Dimensione dello spazio di archiviazione in byte
    int n_file_storage;                                             // Numero massimo di file che possono essere contenuti contemporaneamente nello spazio di archiviazione
    char soc_filename[UNIX_PATH_MAX];                               // Nome del socket file
    int max_conn_wait;                                              // Numero massimo di connessioni in attesa di essere accettate
    int max_active_conn;                                            // Numero massimo di connessioni attive contemporaneamente
    int manager_timeout;                                            // Timeout in millisecondi associato alla poll
    char log_filename[UNIX_PATH_MAX];                               // Filename del file di log
    int client_timeout;                                             // Il timeout per chiudere le connessioni inutilizzate con i client, specificato in secondi
};

typedef struct config_struct config;

/* 
 * Indica quando il server deve terminare le sue attività, se uguale a 0 il server continua le sue operazioni, 
 * se uguale a 1 il server termina immediatamente chiudendo tutte le connessioni attive
 * se uguale a 2 non accetta più nuove richieste ma le connessioni attualmente attive non vengono chiuse finchè tutte le richieste non sono soddisfatte  
 */
volatile sig_atomic_t rcvd_signal = 0;    

static void sigint_manager(int signum) {
    rcvd_signal = SIGINT;
} 

static void sigquit_manager(int signum) {
    rcvd_signal = SIGQUIT;
} 

static void sihup_manager(int signum) {
    rcvd_signal = SIGHUP;
} 

/*
 * Esegue il parsing del file di configurazione
 * Parametri: none
 * Ritorna: struct config contenente i valori di configurazione
 */
config parse_config();

/*
 * Alloca e inizializza l'array contenente i file descriptor di connessioni aperte con i client, utilizzato per la system call poll
 * Parametri:
 *      max: il numero massimo di connessioni contemporaneamente attive
 * Ritorna: l'array di file descriptor inizializzato
 */
struct pollfd *init_fds(int max, time_t **client_lu);

/*
 * Aggiunge un nuovo file descriptor all'array fds
 * Parametri:
 *      fds: l'array in cui inserire il nuovo file descriptor
 *      fd: il file descriptor da aggiungere a fds
 *      max: il numero massimo di connessioni contemporaneamente attive
 *      max_poll_index: l'indice massimo t.c. fds[max_poll_index].fd != -1
 * Ritorna: l'indice il cui è stato inserito il nuovo file descriptor
 */
int add_fd(struct pollfd *fds, int fd, int max);

/*
 * Chiude la connessione con un client rimuovendo il corrispondente file descriptor da fds
 * Parametri:
 *      fds: l'array da cui rimuovere il file descriptor
 *      fd: il file descriptor da rimuovere
 *      max: il numero massimo di connessioni contemporaneamente attive
 *      max_poll_index: l'indice massimo t.c. fds[max_poll_index].fd != -1
 */
int close_conn(struct pollfd *fds, int fd, int max);

int main(int argc, char *argv[]){
    config config;                                                      // Contiene i valori di configurazione del server

    int fd_socket;                                                      // Socket usato dal server per ricevere le richieste di connessione
    int n_fd_socket;                                                    // Il socket usato dal server per la comunicazione con il client

    int poll_result;                                                    // Il risultato ottenuto dall'esecuzione della procedura poll
    int i;
    int active_conn = 0;                                                // Numero di connessioni attualmente attive
    int stat_max_conn = 0;                                              // Statistica del numero massimo di connessioni contemporaneamente attive
    int terminate = 0;

    struct sockaddr_un socket_addr;
    struct pollfd *fds;                                                 // Array di file descriptor su eseguire la poll
    time_t *client_lu;                                                  // Array contenente il momento in cui il client corrispondente ha comunicato per l'ultima volta con il server
    time_t actual_time;

    request_queue_el *head_request = NULL, *tail_request = NULL;        // Testa e coda della coda delle richieste
    resolved_queue_el *head_resolved = NULL, *tail_resolved = NULL;     // Testa e coda della coda delle richieste soddisfatte

    storage storage; 

    pthread_t *workers;                                                 // Array contenente puntatori ai thread worker

    worker_arg *args;                                                   // Struct contenente tutti gli argomenti che devono essere passati ai worker al momento della loro creazione

    resolved_queue_el resolved;                                         // Un singolo elemento della coda delle richieste risolte                                    

    struct sigaction sigint;
    struct sigaction sigquit;
    struct sigaction sighup;

    int *served_request;

    FILE *log_file;

    f_el **ht = NULL;

    config = parse_config();

    // Visualizza la configurazione che è stata letta dal server dal file di configurazione 
    printf("Configurazione letta dal file config.txt:\n");
    printf("\t-Numero di thread worker: %d\n\t-Dimensione dello storage: %fMbytes\n\t-Numero massimo di file: %d\n\t-Filename del socket di ascolto: %s\n\t-Numero massimo di connessioni in attesa: %d\n\t-Numero massimo di connessioni attive contemporaneamente: %d\n\t-Timeout per la poll: %d\n\t-Filename del file di log: %s\n\t-Timeout delle connessioni con i client: %d\n", config.n_thread, (config.b_storage / 1000000), config.n_file_storage, config.soc_filename, config.max_conn_wait, config.max_active_conn, config.manager_timeout, config.log_filename, config.client_timeout);
    
    memset(&sigint, 0, sizeof(sigint));
    memset(&sigquit, 0, sizeof(sigquit));
    memset(&sighup, 0, sizeof(sighup));

    sigint.sa_handler = sigint_manager;
    sigquit.sa_handler = sigquit_manager;
    sighup.sa_handler = sihup_manager;

    if(sigaction(SIGINT, &sigint, NULL) == -1) {
        printf("Manager:");
        perror("Impostando il nuovo handler per SIGINT");

        return -1;
    }

    if(sigaction(SIGQUIT, &sigquit, NULL) == -1) {
        printf("Manager:");
        perror("Impostando il nuovo handler per SIGQUIT");

        return -1;
    }

    if(sigaction(SIGHUP, &sighup, NULL) == -1) {
        printf("Manager:");
        perror("Impostando il nuovo handler per SIGHUP");

        return -1;
    }

    // Crea un socket non bloccante
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

    printf("MANAGER: Socket creato con successo\n");

    // Alloca e inizializza l'array che contiene i file descriptor delle connessioni aperte
    fds = init_fds(config.max_active_conn, &client_lu);
    fds[0].fd = fd_socket;
    fds[0].events = POLLIN;

    // Inizializza lo storage
    ht = malloc((int)((config.n_file_storage * 1.3) + 1) * sizeof(f_el*));
    for(i = 0; i < (int)((config.n_file_storage * 1.3) + 1); i++) {
        ht[i] = NULL;
    }
    storage.size.size_bytes = config.b_storage;
    storage.size.size_n = config.n_file_storage;
    storage.size.size_ht = (int)(((int)config.n_file_storage * 1.3) + 1);
    storage.size.occupied_bytes = 0;
    storage.size.occupied_size_n = 0;
    storage.ht = ht;
    storage.statistics.max_stored_bytes = 0;
    storage.statistics.max_stored_files = 0;
    storage.statistics.replaced_files = 0;
    storage.log_filename = config.log_filename;

    //Alloca l'array per contenere i riferimenti ai thread worker presenti nel thread pool
    workers = malloc(config.n_thread * sizeof(pthread_t));
    served_request = malloc(config.n_thread * sizeof(pthread_t));

    // Alloca e inizializza gli argomenti dei thread worker
    args = malloc(sizeof(worker_arg));
    args->head_request = &head_request;
    args->head_resolved = &head_resolved;
    args->tail_resolved = &tail_resolved;
    args->storage = &storage;
    args->max_conn = config.max_active_conn;

    // Crea e avvia i thread worker del thread pool
    for(i = 0; i < config.n_thread; i++) {
        args->thread_n = i;
        served_request[i] = 0;
        args->served_request = served_request + i;

        if((errno = pthread_create(&(workers[i]), NULL, &main_worker, args)) != 0) {
            perror("MANAGER: Creando i thread worker");
        }
    }

    printf("MANAGER: Thread pool creato correttamente\n");
    printf("MANAGER: Il server è pronto\n\n");

    while(!terminate) {
        if(rcvd_signal == SIGINT || rcvd_signal == SIGQUIT) {
            terminate = 1;

            close(fd_socket);
        } else if(rcvd_signal == SIGHUP){
            if(active_conn == 0) {
                terminate = 2;

                close(fd_socket);
                fds[0].fd = -1;
            }

            memset(&sigint, 0, sizeof(sigint));

            sigint.sa_handler = SIG_IGN;

            memset(&sigquit, 0, sizeof(sigint));

            sigquit.sa_handler = SIG_IGN;

            if(sigaction(SIGINT, &sigint, NULL) == -1) {
                printf("Manager:");
                perror("Impostando il nuovo handler per SIGINT");
            }

            if(sigaction(SIGQUIT, &sigquit, NULL) == -1) {
                printf("Manager:");
                perror("Impostando il nuovo handler per SIGQUIT");
            }
        }

        if(terminate != 1) {
            // Verifica se qualche connessione è pronta per poter essere letta
            poll_result = poll(fds, config.max_active_conn + 1, config.manager_timeout);

            if(poll_result == -1) {
                if(errno != EINTR) {
                    perror("MANAGER: Polling");
                }
            }

            if(poll_result > 0) {
                if(fds[0].fd != -1 && active_conn < config.max_active_conn) {
                    if(fds[0].revents == POLLIN) {
                        n_fd_socket = accept(fd_socket, NULL, 0);

                        if(n_fd_socket == -1) {
                            if(errno != EAGAIN && errno != EWOULDBLOCK) {
                                perror("MANAGER: Accettando una nuova connessione");
                            }
                        } else {
                            printf("MANAGER: Nuova connessione accettata, il nuovo descrittore del socket è: %d\n", n_fd_socket);

                            i = add_fd(fds, n_fd_socket, config.max_active_conn);

                            client_lu[i] = time(NULL);

                            active_conn++;

                            if(active_conn > stat_max_conn) {
                                stat_max_conn = active_conn;
                            }
                        }
                    }
                }

                // Cerca i file descriptor che sono pronti per la lettura
                for(i = 1; i < config.max_active_conn + 1; i++) {
                    if(fds[i].revents == POLLIN) {
                        // Serve per evitare che il manager vada a chiudere una connessione in uso da parte dei worker nel caso in cui l'operazione richiedesse un tempo maggiore al timeout
                        client_lu[i] = -1;

                        // Si inseriscono i file descriptor nella coda delle richieste
                        if(push_request(&head_request, &tail_request, fds[i].fd) == NULL) {
                            perror("MANAGER: Inserendo una nuova richiesta");
                        }

                        // Si ignora il file descriptor per successive call di poll
                        fds[i].fd = -fds[i].fd;
                    }
                }
            }

            errno = 0;
            resolved = pop_resolved(&head_resolved);
            while(resolved.resolved_fd != -1) {
                if(resolved.close) {
                    printf("MANAGER: La connessione con %d è chiusa\n", resolved.resolved_fd);

                    close(resolved.resolved_fd);

                    // Qui non serve inizializzare client_lu in quanto prima di passare il descrittore ai worker la corrispondente cella in client_lu è impostata a -1
                    clean_closed_conn(&storage, resolved.resolved_fd, config.max_active_conn);

                    close_conn(fds, resolved.resolved_fd, config.max_active_conn);

                    active_conn--;

                    printf("MANAGER: Rimangono %d connessioni attive\n", active_conn);
                } else {
                    for(i = 1; i < config.max_active_conn + 1; i++) {
                        if(-fds[i].fd == resolved.resolved_fd) {
                            fds[i].fd = -fds[i].fd;

                            client_lu[i] = time(NULL);
                        }
                    }
                }

                resolved = pop_resolved(&head_resolved);
            }

            if(resolved.resolved_fd == -1 && errno != 0) {
                perror("MANAGER: Leggendo la coda delle richieste risolte");
            }

            // Verifica se il timer è scaduto per qualche connessione
            actual_time = time(NULL);
            for(i = 0; i < config.max_active_conn; i++) {
                if(client_lu[i] != -1 && (actual_time - client_lu[i]) >= config.client_timeout) {
                    printf("MANAGER: La connessione con %d è chiusa per timeout\n", fds[i].fd);

                    close(fds[i].fd);

                    client_lu[i] = -1;

                    clean_closed_conn(&storage, -fds[i].fd, config.max_active_conn);

                    close_conn(fds, -fds[i].fd, config.max_active_conn);

                    active_conn--;

                    printf("MANAGER: Rimangono %d connessioni attive\n", active_conn);
                }
            }
        }
    }

    // Operazioni per la terminazione del server
    for(i = 0; i < config.n_thread; i++) {
        pthread_cancel(workers[i]);
        pthread_join(workers[i], NULL);
        printf("MANAGER: Worker %d, terminato\n", i);
    }

    print_ht(storage.ht, storage.size.size_ht);

    printf("\nStatistiche: \n");
    printf("\t-Numero massimo di file memorizzati: %d\n", storage.statistics.max_stored_files);
    printf("\t-Numero massimo di byte memorizzati: %fMbytes\n", (double)storage.statistics.max_stored_bytes / 1000000);
    printf("\t-Numero di file rimpiazzati: %d\n", storage.statistics.replaced_files);
    printf("\t-Numero di file attualmente memorizzati nello storage: %d\n", storage.size.occupied_size_n);
    printf("\t-Numero di byte attualmente memorizzati nello storage: %fMbytes\n", (double)storage.size.occupied_bytes / 1000000);

    log_file = fopen(storage.log_filename, "a");

    fwrite("maxsize:", sizeof(char), 8, log_file);
    fprintf(log_file, "%ld", storage.statistics.max_stored_bytes);
    fwrite("\n", sizeof(char), 1, log_file);

    fwrite("maxnsize:", sizeof(char), 9, log_file);
    fprintf(log_file, "%d", storage.statistics.max_stored_files);
    fwrite("\n", sizeof(char), 1, log_file);

    fwrite("replacedfiles:", sizeof(char), 14, log_file);
    fprintf(log_file, "%d", storage.statistics.replaced_files);
    fwrite("\n", sizeof(char), 1, log_file);

    for(i = 0; i < config.n_thread; i++) {
        fwrite("servedrequest:", sizeof(char), 14, log_file);
        fprintf(log_file, "%d", i);
        fwrite(",", sizeof(char), 1, log_file);
        fprintf(log_file, "%d", served_request[i]);
        fwrite("\n", sizeof(char), 1, log_file);
    }

    fwrite("maxactiveconn:", sizeof(char), 14, log_file);
    fprintf(log_file, "%d", stat_max_conn);
    fwrite("\n", sizeof(char), 1, log_file);

    fclose(log_file);

    free_request_queue(head_request);
    free_resolved_queue(head_resolved);
    free_ht(storage.ht, storage.size.size_ht);

    free(ht);
    free(workers);
    free(fds);
    free(client_lu);
    free(served_request);
    free(args);

    return 0;
}

config parse_config() {
    FILE *conf_fp;                                                  //Puntatore al file di configurazione
    char *tag_name, *value;
    config result;

    if(access(CONFIG_FN, R_OK) == -1) {
        // Verifica l'esistenza del file di configurazione
        if(errno == ENOENT) {
            // Il file non esiste
            mkdir(CONFIG_PATH,S_IRWXU);

            printf("Creating config.txt file...\n");
            conf_fp = fopen(CONFIG_FN, "w");

            // Inizializza il file config.txt con la configurazione dei default
            printf("Initializing configuration file...\n\n");
            fwrite(DEFAULT_CONFIG, strlen(DEFAULT_CONFIG), sizeof(char), conf_fp);

            fclose(conf_fp);
        }
    }

    // Il file esiste o è stato creato

    conf_fp = fopen(CONFIG_FN, "r");

    char *buffer = malloc(BUFFER_SIZE * sizeof(char));

    if(buffer == NULL) {
        perror("Allocando il buffer per leggere il file di config:");
    }

    // Legge il contenuto del file config.txt
    while(!feof(conf_fp)) {
        // Ottiene una riga del file
        if(fgets(buffer, BUFFER_SIZE, conf_fp) == NULL) {
            // Verifica la presenza di errori
            if(!feof(conf_fp)) {
                if(ferror(conf_fp)) {
                    perror("Leggendo il file di configurazione:");

                    fclose(conf_fp);

                    return result;
                }
            }
        } else {
            if(buffer[0] != '#') {
                tag_name = strtok(buffer, TOKEN_SYMBOL);
                value = strtok(NULL, TOKEN_SYMBOL);

                // Imposta i campi della struct usata per la memorizzazione delle impostazioni
                if(!strcmp(tag_name, "n_thread")) {
                    result.n_thread = (int)(strtol(value, NULL, 10));

                } else if(!strcmp(tag_name, "b_storage")) {
                    result.b_storage = strtod(value, NULL) * 1000000.0f;

                } else if(!strcmp(tag_name, "n_file_storage")) {
                    result.n_file_storage = (int)(strtol(value, NULL, 10));

                } else if(!strcmp(tag_name, "soc_filename")) {
                    strcpy(result.soc_filename, value);
                    result.soc_filename[strcspn(result.soc_filename, "\n")] = '\0';
                } else if(!strcmp(tag_name, "max_conn_wait")) {
                    result.max_conn_wait = (int)(strtol(value, NULL, 10));

                } else if(!strcmp(tag_name, "max_active_conn")) {
                    result.max_active_conn = (int)(strtol(value, NULL, 10));

                } else if(!strcmp(tag_name, "manager_timeout")) {
                    result.manager_timeout = (int)(strtol(value, NULL, 10));

                } else if(!strcmp(tag_name, "log_filename")) {
                    strcpy(result.log_filename, value);

                    if(result.log_filename[strlen(result.log_filename) - 1] == '\n') {
                        result.log_filename[strlen(result.log_filename) - 1] = '\0';
                    }
                } else if(!strcmp(tag_name, "client_timeout")) {
                    result.client_timeout = (int)(strtol(value, NULL, 10));

                } else {
                    printf("L'impostazione non è supportata, controlla il file di configurazione: %s\n", tag_name);
                }
            }
        }
    }

    fclose(conf_fp);

    free(buffer);

    return result;
}

struct pollfd *init_fds(int max, time_t **client_lu) {
    struct pollfd *fds;
    int i;

    fds = malloc((max + 1) * sizeof(struct pollfd));
    *client_lu = malloc((max + 1) * sizeof(time_t));

    for(i = 0; i < max + 1; i++) {
        fds[i].fd = -1;
        fds[i].events = 0;
        fds[i].revents = 0;
    }

    for(i = 0; i < max + 1; i++) {
        *(*client_lu + i) = -1;
    }


    return fds;
}

int add_fd(struct pollfd *fds, int fd, int max) {
    int i;

    for(i = 1; i < max + 1; i++) {
        if(fds[i].fd == -1) {
            fds[i].fd = fd;
            fds[i].events = POLLIN;

            return i;
        }
    }

    return -1;
}

int close_conn(struct pollfd *fds, int fd, int max) {
    int i;

    // Cerca la cella dell'array che contiene il file descriptor da rimuovere
    for(i = 1; i < max + 1; i++) {
        if(-fds[i].fd == fd) {
            fds[i].fd = -1;
            fds[i].events = 0;

            return 0;
        }
    }

    return -1;
}
