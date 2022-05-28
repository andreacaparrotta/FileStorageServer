#include <limits.h>
#define UNIX_PATH_MAX 108

struct worker_arg{
    request_queue_el **head_request;            // Il puntatore al puntatore alla testa della queue da cui ottenere i file descriptor pronti per la lettura
    resolved_queue_el **head_resolved;          // Il puntatore al puntatore alla testa della queue in cui inserire i file descriptor riguardanti richieste elaborate
    resolved_queue_el **tail_resolved;          // Il puntatore al puntatore alla coda della queue in cui inserire i file descriptor riguardanti richieste elaborate
    storage *storage;                           // Il puntatore alla struct che modella lo storage
    int thread_n;                               // Il numero identificativo del worker
    int max_conn;                               // Il numero massimo di connessioni che possono essere attive contemporaneamente
    int *served_request;                        // Il puntatore al contatore di richieste elaborate dal worker
};

typedef struct worker_arg worker_arg;

/*
 * Funzione che implementa il funzionamento dei thread worker
 * Paramentri:
 *      arg: argomenti passati ai worker, per la descrizione vedi sopra(definizione struct worker_arg)
 * Ritorna: none
 */
void *main_worker(void *arg);

/*
 * Verifica la tipologia di richiesta ricevuta e la gestisce in modo appropriato
 * Parametri: 
 *      storage: il puntatore allo storage su cui eseguire le operazioni richieste
 *      request: la richiesta ricevuta dal client
 *      socket_fd: il file descriptor del socket da cui si è ricevuta la richiesta
 *      max: il numero massimo di connessioni che possono essere attive contemporaneamente
 * Ritorna: 0 se la richiesta è soddisfatta correttamente, -1 in caso di errore,  imposta errno adeguatamente
 */
int check_request(storage *storage, char *request, int socket_fd, int max);

static void cleanup_handler(void *arg);

void *main_worker(void *arg) {
    worker_arg *args = (worker_arg *)arg;

    request_queue_el **head_request = args->head_request;
    resolved_queue_el **head_resolved = args->head_resolved;
    resolved_queue_el **tail_resolved = args->tail_resolved;
    storage *storage = args->storage;
    int *served_request = (int *)args->served_request;
    int thread_n = args->thread_n;
    int max_conn = args->max_conn;

    int request_size;
    void *request;
    int socket_fd;
    int result;
    int o_state;
    
    struct sigaction sigpipe;
    memset(&sigpipe, 0, sizeof(sigpipe));
    sigpipe.sa_handler = SIG_IGN;
    if(sigaction(SIGPIPE, &sigpipe, NULL) == -1) {
        printf("WORKER %d:", thread_n);
        perror("Impostando il nuovo handler sigpipe");

        pthread_exit((void *)1);
    }

    pthread_cleanup_push(cleanup_handler, NULL);

    while(1) {
        result = 0;

        // Ottiene un file descriptor pronto per essere letto, oppure si mette in attesa in attesa che uno diventi pronto
        if((socket_fd = pop_request(head_request)) == -1) {
            printf("WORKER %d:", thread_n);
            perror("Ottenendo la richiesta: ");
        } else {
            // Disattiva la possibilità di interrompere il worker fino a che la richiesta non è soddisfatta completamente, necessario per evitare che il sistema venga lasciato in uno stato inconsistente
            pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &o_state);

            // Legge la dimensione della richiesta
            if(read(socket_fd, &request_size, sizeof(int)) == -1) {
                printf("WORKER %d: ERRORE socket: %d", thread_n, socket_fd);
                perror("Leggendo dal socket");

                result = 1;
            } else {
                request = malloc(request_size * sizeof(char));
                memset(request, 0, request_size);

                // Legge la richiesta
                if(read(socket_fd, request, request_size) == -1) {
                    printf("WORKER %d:", thread_n);
                    perror("Leggendo dal socket");

                    result = 1;
                }
            }

            printf("WORKER %d: ha ricevuto la richiesta %c, dal socket: %d \n", thread_n, ((char *)request)[0], socket_fd);

            if(result != 1) {
                // Elabora la richiesta
                result = check_request(storage, (char *)request, socket_fd, max_conn);

                *served_request += 1;

                free(request);
            }

            if(result == -1) {
                // Si è verificato un errore 

                printf("WORKER %d:", thread_n);
                perror("Elaborando la richiesta");
            } else if(result == 0) {
                // Richiesta soddisfatta, la connessione rimane aperta per altre richieste

                if(push_resolved(head_resolved, tail_resolved, socket_fd, 0) == NULL) {
                    printf("WORKER %d:", thread_n);
                    perror("Inserendo la richiesta soddisfatta");
                }
            } else if(result == 1) {
                // È arrivata una richiesta di chiusura della connessione, la connessione deve essere chiusa
                if(push_resolved(head_resolved, tail_resolved, socket_fd, 1) == NULL) {
                    printf("WORKER %d:", thread_n);
                    perror("Inserendo la richiesta soddisfatta");
                }
            }

            // Ripristina la possibilità di cancellare il worker, ora lo stato rimane consistente
            pthread_setcancelstate(o_state, &o_state);
        }
    }

    pthread_cleanup_pop(1);
}

int check_request(storage *storage, char *request_m, int socket_fd, int max) {
    f_el *victims;
    f_el *victim;

    char *request_code;

    char *pathname;
    char *content;
    char *flags_string;
    int flags;
    int n;
    char *read_file;

    char *response_m;
    int response_size = 0;
    int result;
    char *save_tok;

    char delimiter[2] = {1, '\0'};

    // Estrae la tipologia della richiesta da request_m
    request_code = strtok_r(request_m, delimiter, &save_tok);

    // Verifica il tipo di richiesta, la elabora e genera il messaggio di risposta
    if(request_code != NULL && strcmp(request_code, CLOSECONN) == 0){
        // È richiesta la chiusura della connessione
        response_size = 2;

        response_m = malloc(2 * sizeof(char));

        strcpy(response_m, SUCCESS);

        result = 1;
    } else if(request_code != NULL && strcmp(request_code, OPENFILE) == 0) {
        // È richiesta l'apertura di un file
        pathname = strtok_r(NULL, delimiter, &save_tok);
        flags_string = strtok_r(NULL, delimiter, &save_tok);
        flags = (int)strtol(flags_string, NULL, 10);

        errno = 0;
        victim = openFile(storage, pathname, flags, socket_fd, max);

        // Genera il messaggio di risposta
        if(errno == 0) {
            if(victim == NULL) {
                response_size = 2;

                response_m = malloc(2 * sizeof(char));

                strcpy(response_m, SUCCESS);
            } else {
                response_size = 2 + strlen(victim->metadata.filename) + 1 + strlen(victim->data) + 1;

                response_m = malloc(response_size + sizeof(char));

                strcpy(response_m, SUCCESS);
                strcat(response_m, delimiter);
                strcat(response_m, victim->metadata.filename);
                strcat(response_m, delimiter);
                strcat(response_m, victim->data);

                free(victim->data);
                free(victim);
            }

            result = 0;
        } else {
            result = -1;
        }
    } else if(request_code != NULL && strcmp(request_code, CLOSEFILE) == 0) {
        // È richiesta la chiusura di un file
        pathname = strtok_r(NULL, delimiter, &save_tok);

        result = closeFile(storage, pathname, socket_fd, max);

        // Genera il messaggio di risposta
        if(result == 0) {
            response_size = 2;

            response_m = malloc(2 * sizeof(char));

            strcpy(response_m, SUCCESS);

            result = 0;
        } else {
            result = -1;
        }
    } else if(request_code != NULL && strcmp(request_code, WRITEFILE) == 0) {
        // È richiesta la scrittura di un file
        int i = 0;

        pathname = strtok_r(NULL, delimiter, &save_tok);
        content = strtok_r(NULL, delimiter, &save_tok);

        errno = 0;
        victims = writeFile(storage, pathname, socket_fd, content, max);

        // Genera il messaggio di risposta
        if(victims != NULL || (victims == NULL && errno == 0)) {
            // Calcola la dimensione della risposta
            if(victims == NULL) {
                response_size = 2;

                response_m = malloc(2 * sizeof(char));

                strcpy(response_m, SUCCESS);
            } else {
                response_size = 2;

                i = 0;
                while(victims[i].data != NULL) {
                    response_size += 1;
                    response_size += strlen(victims[i].metadata.filename);
                    response_size += 1;
                    response_size += strlen(victims[i].data);

                    i++;
                }

                response_m = malloc((response_size) * sizeof(char));

                // Scrive il messaggio di risposta
                strcpy(response_m, SUCCESS);
                i = 0;
                while(victims[i].data != NULL) {
                    strcat(response_m, delimiter);
                    strcat(response_m, victims[i].metadata.filename);
                    strcat(response_m, delimiter);
                    strcat(response_m, victims[i].data);

                    free(victims[i].data);
                    i++;
                }
                
                free(victims);
            }

            result = 0;
        } else {
            result = -1;
        }
    } else if(request_code != NULL && strcmp(request_code, READFILE) == 0) {
        // È richiesta la lettura di un file
        pathname = strtok_r(NULL, delimiter, &save_tok);

        content = readFile(storage, pathname, socket_fd);

        // Genera il messaggio di risposta
        if(content != NULL) {
            response_size = 3 + strlen(content);

            response_m = malloc(response_size * sizeof(char));

            strcpy(response_m, SUCCESS);
            strcat(response_m, delimiter);
            strcat(response_m, content);

            result =  0;
        } else {
            result = -1;
        }
    } else if(request_code != NULL && strcmp(request_code, READNFILE) == 0){
        // È richiesta la lettura di n file
        n = (int)strtol(strtok_r(NULL, delimiter, &save_tok), NULL, 10);

        read_file = readNFiles(storage, n, socket_fd);

        // Genera il messaggio di risposta
        if(read_file != NULL) {
            response_size = 3 + strlen(read_file);

            response_m = malloc(response_size * sizeof(char));

            strcpy(response_m, SUCCESS);
            strcat(response_m, delimiter);
            strcat(response_m, read_file);

            free(read_file);

            result = 0;
        } else {
            result = -1;
        }
    } else if(request_code != NULL && strcmp(request_code, LOCKFILE) == 0) {
        // È richiesta l'acquisizione della lock su un file
        pathname = strtok_r(NULL, delimiter, &save_tok);

        result = lockFile(storage, pathname, socket_fd);

        // Genera il messaggio di risposta
        if(result == 0) {
            response_size = 2;

            response_m = malloc(2 * sizeof(char));

            strcpy(response_m, SUCCESS);

            result = 0;
        } else {
            result = -1;
        }
    } else if(request_code != NULL && strcmp(request_code, UNLOCKFILE) == 0) {
        // È richiesto il rilascio della lock su un file
        char *pathname = strtok_r(NULL, delimiter, &save_tok);

        result = unlockFile(storage, pathname, socket_fd);
        if(result == 0) {
            response_size = 2;

            response_m = malloc(2 * sizeof(char));

            strcpy(response_m, SUCCESS);

            result = 0;
        } else {
            result = -1;
        }
    } else if(request_code != NULL && strcmp(request_code, REMOVEFILE) == 0) { 
        // È richiesta la rimozione di un file
        char *pathname = strtok_r(NULL, delimiter, &save_tok);

        result = removeFile(storage, pathname, socket_fd);

        // Genera il messaggio di risposta
        if(result == 0) {
            response_size = 2;

            response_m = malloc(2 * sizeof(char));

            strcpy(response_m, SUCCESS);

            result = 0;
        } else {
            result = -1;
        }

        if((pthread_mutex_lock(&lock_storage)) != 0) {
            print_ht(storage->ht, storage->size.size_ht);
        }

        pthread_mutex_unlock(&lock_storage);
    } else if(request_code != NULL && strcmp(request_code, APPENDFILE) == 0) {
        // È richiesta l'operazione di scrittura in concatenazione al file
        int i = 0;

        pathname = strtok_r(NULL, delimiter, &save_tok);
        content = strtok_r(NULL, delimiter, &save_tok);

        errno = 0;
        victims = appendToFile(storage, pathname, socket_fd, content, max);

        // Genera il messaggio di risposta
        if(victims != NULL || (victims == NULL && errno == 0)) {
            // Calcola la dimensione della risposta
            if(victims == NULL) {
                response_size = 2;

                response_m = malloc(2 * sizeof(char));

                strcpy(response_m, SUCCESS);
            } else {
                response_size = 2;

                i = 0;
                while(victims[i].data != NULL) {
                    response_size += 1;
                    response_size += strlen(victims[i].metadata.filename);
                    response_size += 1;
                    response_size += strlen(victims[i].data);

                    i++;
                }

                response_m = malloc((response_size) * sizeof(char));

                // Scrive il messaggio di risposta
                strcpy(response_m, SUCCESS);
                i = 0;
                while(victims[i].data != NULL) {
                    strcat(response_m, delimiter);
                    strcat(response_m, victims[i].metadata.filename);
                    strcat(response_m, delimiter);
                    strcat(response_m, victims[i].data);

                    i++;
                }

                free(victims);
            }

            result = 0;
        } else {
            result = -1;
        }
    } else if(request_code != NULL && strcmp(request_code, WRITE_NO_CONTENT) == 0) {
            write_no_content(storage);
            
            response_size = 2;

            response_m = malloc(3 * sizeof(char));

            strcpy(response_m, SUCCESS);

            result = 0;
    } else {
        errno = EINVAL;

        result = -1;
    }

    if(result == -1) {
        // Si è verificato un errore
        response_size = 2;

        response_m = malloc(2 * sizeof(char));

        // Scrive il codice di errore nel messaggio di risposta
        switch(errno) {
            case EBADR:
                strcpy(response_m, ALREADY_OPENED);

                result = 0;

                break;
            case ENOENT:
                strcpy(response_m, FILE_NOT_EXIST);

                result = 0;

                break;
            case EINVAL:
                strcpy(response_m, UNKNOWN);

                result = 0;

                break;
            case ENAMETOOLONG:
                strcpy(response_m, FILENAME_TOO_LONG);

                result = 0;

                break;
            case EEXIST:
                strcpy(response_m, FILE_ALREADY_EXIST);

                result = 0;

                break;
            case EBADF:
                strcpy(response_m, FILE_NOT_OPENED);

                result = 0;
                
                break;
            case EPERM:
                strcpy(response_m, FILE_LOCKED);

                result = 0;
                
                break;
            case ENOMEM:
                strcpy(response_m, NOT_ENO_MEM);

                result = 0;

                break;
        }
    } 

    // Invia la dimensione della risposta al client
    if(write(socket_fd, &response_size, sizeof(int)) == -1) {
        perror("WORKER: Scrivendo al client");

        result = 1;
    } else {
        // Invia il messaggio di risposta al client
        if(write(socket_fd, response_m, response_size) == -1) {
            perror("WORKER: Scrivendo al client");
                
            result = 1;
        }
    }

    free(response_m);

    return result;
}

static void cleanup_handler(void *arg) {
    pthread_mutex_unlock(&lock_queue);
}