#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <sys/un.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>

#include "definitions.h"

#define UNIX_PATH_MAX 108
#define RESPONSE_BUFF_SIZE 2

struct request_args {
    char *pathname;
    int flags;
    char *n;
    char *content;
    int size;
};

struct response_args {
    void **buf;
    size_t *size;
};

typedef struct request_args request_args;
typedef struct response_args response_args;

char *sel_socketname = NULL;                                                    // Socket filename usato per apertura della connessione
int socket_fd = -1;                                                             // File descriptor del socket file
int openfile = 0;                                                               // Indica se l'ultima operazione terminata con successo è openFile(, O_CREATE | O_LOCK)
char *last_request_target = NULL;                                               // Indica il filename dell'ultima a cui si riferisce l'ultima operazione openFile(, O_CREATE | O_LOCK)
char *sel_dirname = NULL;                                                       // Indica la directory in cui salvare i file inviati dal server
int print_upper_r = 0;                                                          // Indica se la verbose mode è richiesta

/*
 * Abilita la modalità verbose per l'operazione -R, necessario per poter fornire informazioni per ogni singolo file letto
 */
void set_p();

/*
 * Imposta la directory di salvataggio dei file espulsi dal server, necessario nel caso dell'espulsione di file a causa della creazione di un file,
 * con flag O_CREATE
 * Parametri:
 *      dirname: il percorso della directory in cui salvare eventuali file espulsi dal server
 */
void set_dirname(char *dirname);

/*
 * Resetta la directory di salvataggio dei file file espulsi
 */
void reset_dirname();

/*
 * Implementa il salvataggio di file letti dal server oppure espulsi a seguito di un'operazione di scrittura
 * Parametri:
 *      file_list: la lista dei file da memorizzare sullo storage, con il corrispondente contenuto
 * Ritorna: 0 in caso di successo, -1 altrimenti
 */
int save_file(char *file_list);

/*
 * Imposta la variabile openfile, necessaria per verificare se l'operazione precedente alla writeFile è stata openFile(pathname, O_CREATE | O_LOCK)
 * Parametri:
 *      n_val: il nuovo valore per la variabile openfile, 1 se l'utima operazione conclusa con successo è stata openFile(pathname, O_CREATE | O_LOCK), 0 altrimenti
 *      pathname: il pathname del file su cui è stata eseguita l'operazione openFile
 */
void set_openfile(int n_val, char *pathname);

/*
 * Gestisce la generazione del messaggio di richiesta e l'invio del messaggio al server
 * Parametri: 
 *      type: la tipologia dell'operazione che deve inviare la richiesta al server, le tipologie sono definite in definitions.h
 *      args: eventuali argomenti da includere nella richiesta
 * Errno: 
 *      EINVAL: se type è NULL
 *      vedi man write per altri errno impostati da write
 * Ritorna: 0 in caso di successo, -1 in caso di errore
 */
int send_request(char *type, request_args *args);

/*
 * Gestisce la ricezione e l'elaborazione del messaggio di risposta del server
 * Parametri:
 *      type: la tipologia dell'operazione che deve ricevere la risposta dal server, le tipologie sono definite in definitions.h
 *      args: eventuali argomenti in cui inserire i risultati ricevuti dal server
 * Errno: 
 *      EBADR: se il file è stato già aperto sul server da questo client
 *      ENOENT: se il file della richiesta non esiste sul server
 *      EINVAL: se si è verificato un errore sconosciuto sul server
 *      ENAMETOOLONG: se il/i filename è/sono di lunghezza eccessiva
 *      EEXIST: si il file specificato esiste già sul server
 *      EBADF: se il file non è stato aperto prima dell'operazione
 *      EPERM: se la lock del file è posseduta da un altro utente
 *      ENOMEM: se il file è troppo grande per poter essere memorizzato sul server
 *      vedi man read per errno impostati da read
 * Ritorna: 0 in caso di successo, -1 in caso di successo
 */
int manage_response(char *type, response_args *args);

/*
 * Connette il client con il server con un socket AF_UNIX
 * Parametri:
 *      socketname: il filename del socket del server a cui connettere il client
 *      msec: il numero di millisecondi da attendere tra una richiesta di connessione fallita
 *            e il tentativo successivo
 *      abstime: il limite di tempo assoluto dopo il quale l'operazione di connessione fallisce
 * Ritorna: 0 se la connessione ha successo, -1 in caso di fallimento, imposta errno adeguatamente
 * Errno:
 *      EISCONN: se la connessione è già aperta
 *      ETIMEDOUT: se la connessione non è stata aperta entro il timeout
 */
int openConnection(const char *socketname, int msec, const struct timespec abstime);

/*
 * Invia al server una richiesta di chiusura della connessione e chiude il socket
 * Parametri:
 *      socketname: il filename del socket del server con cui chiudere la connessione
 * Ritorna: 0 se la chiusura della connessione ha successo, -1 in caso di fallimento, imposta errno adeguatamente
 * Errno:
 *      ENOTCONN: se la connessione è già chiusa
 *      EINVAL: se socketname != sel_socketname
 */
int closeConnection(const char *socketname);

/*
 * Invia al server una richiesta di apertura del file con filename == pathname
 * Parametri:
 *      pathname: il nome del file di cui richiedere l'apertura
 *      flags:
 *          O_CREATE: se specificato e il file non esiste, viene creato. Nel caso in cui il file esiste e il flag
 *                    è specificato viene restituito un errore
 *          O_LOCK: se specificato il file viene aperto o creato in modalità locked
 * Errno:
 *      ENOTCONN: se il client non ha aperto la connessione con il server
 *      ENOENT: se il file della richiesta non esiste sul server
 *      EBADR: se il file è stato già aperto sul server da questo client
 *      EINVAL: se si è verificato un errore sconosciuto sul server
 *      ENAMETOOLONG: se il/i filename è/sono di lunghezza eccessiva
 *      EEXIST: si il file specificato esiste già sul server
 * Ritorna: 0 in caso di successo, -1 in caso di errore, imposta errno adeguatamente
 */
int openFile(const char *pathname, int flags);

/*
 * Invia al server una richiesta di chiusura del file con filename == pathname
 * Parametri:
 *      pathname: il nome del file di cui richiedere la chiusura
 * Errno:
 *      ENOTCONN: se il client non ha aperto la connessione con il server
 *      ENOENT: se il file della richiesta non esiste sul server
 *      EINVAL: se si è verificato un errore sconosciuto sul server
 *      ENAMETOOLONG: se il/i filename è/sono di lunghezza eccessiva
 *      EBADF: se il file non è stato aperto prima dell'operazione
 * Ritorna: 0 in caso di successo, -1 in caso di errore, imposta errno adeguatamente
 */
int closeFile(const char *pathname);

/*
 * Invia al server una richiesta di lettura del file con filename == pathname
 * Parametri:
 *      pathname: il nome del file di cui richiedere la lettura
 *      buf: buffer in cui memorizzare il contenuto del file
 *      size: dimensione del contenuto del file in byte
 * Errno:
 *      ENOTCONN: se il client non ha aperto la connessione con il server
 *      ENOENT: se il file della richiesta non esiste sul server
 *      EINVAL: se si è verificato un errore sconosciuto sul server
 *      ENAMETOOLONG: se il/i filename è/sono di lunghezza eccessiva
 *      EBADF: se il file non è stato aperto prima dell'operazione
 *      EPERM: se la lock sul file è posseduta da un altro utente
 * Ritorna: 0 in caso di successo, -1 in caso di errore, imposta errno adeguatamente
 */
int readFile(const char *pathname, void **buf, size_t *size);

/*
 * Invia al server una richiesta di lettura di n file tra quelli presenti nello storage
 * Parametri: 
 *      n: il numero di file da leggere nel server, se n <= 0 allora si vuole leggere l'intero contenuto del server
 *      dirname: directory in cui salvare i file letti dal server
 * Errno:
 *      ENOTCONN: se il client non ha aperto la connessione con il server
 *      EINVAL: se si è verificato un errore sconosciuto sul server
 *      ENAMETOOLONG: se il pathname della directory in cui salvare i file è troppo lungo
 * Ritorna: 0 in caso di successo, -1 in caso di errore, imposta ernno adeguatamente
 */
int readNFiles(int n, const char *dirname);

/*
 * Invia al server una richiesta di scrittura del contenuto del file con filename == pathname
 * Parametri:
 *      pathname: il nome del file in cui scrivere il contenuto
 *      dirname: la directory sulla macchina, su cui viene eseguito il client, in cui salvare eventuali file rimossi dal server per mancanza di spazio
 * Errno:
 *      ENOTCONN: se il client non ha aperto la connessione con il server
 *      ENOENT: se il file della richiesta non esiste sul server
 *      EINVAL: se si è verificato un errore sconosciuto sul server
 *      ENAMETOOLONG: se il/i filename è/sono di lunghezza eccessiva
 *      EBADF: se il file non è stato aperto prima dell'operazione, oppure se open_filename non è stato imposrato prima dell'operazione
 *      EPERM: se la lock sul file è posseduta da un altro utente
 *  Ritorna: 0 in caso di successo,  -1 in caso di errore, imposta errno adeguatamente
 */
int writeFile(const char *pathname, const char *dirname);

/*
 * Invia al server una richiesta di scrittura in modalità append del file con filename == pathname 
 * Parametri:
 *      pathname: il nome del file in cui accodare il contenuto
 *      buf: il buffer che contiene i byte da accodare al file
 *      size: la dimensione del buffer
 *      dirname: la directory sulla macchina, su cui viene eseguito il client, in cui salvare eventuali file rimossi dal server per mancanza di spazio
 * Errno:
 *      ENOTCONN: se il client non ha aperto la connessione con il server
 *      ENOENT: se il file della richiesta non esiste sul server
 *      EINVAL: se si è verificato un errore sconosciuto sul server
 *      ENAMETOOLONG: se il/i filename è/sono di lunghezza eccessiva
 *      EBADF: se il file non è stato aperto prima dell'operazione, oppure se open_filename non è stato imposrato prima dell'operazione
 *      EPERM: se la lock sul file è posseduta da un altro utente
 * Ritorna: 0 in caso di successo, -1 in caso di errore, imposta errno adeguatamente
 */
int appendToFile(const char *pathname, void *buf, size_t size, const char *dirname);

/*
 * Invia al server una richiesta di acquisizione della lock del file con filename == pathname
 * Parametri:
 *      pathname: il percorso assoluto del file su cui acquisire la lock
 * Errno:
 *      ENOTCONN: se il client non ha una connessione aperta con il server
 *      ENAMETOOLONG: se il percorso specificato ha una lunghezza maggiore di UNIX_PATH_MAX
 *      EINVAL: nel caso di un errore sconosciuto del server
 *      ENOENT: nel caso in cui il file specificato non esiste nel server
 *      EPERM: nel caso in cui la lock sul file specificato è acquisita da un altro client 
 * Ritorna: 0 in caso di successo, -1 in caso di errore, imposta errno adeguatamente
 */
int lockFile(const char *pathname);

/*
 * Invia al server una richiesta di rilascio della lock del file con filename == pathname
 * Parametri:
 *      pathname: il percorso assoluto del file su cui rilasciare la lock
 * Errno:
 *      ENOTCONN: se il client non ha una connessione aperta con il server
 *      ENAMETOOLONG: se il percorso specificato ha una lunghezza maggiore di UNIX_PATH_MAX
 *      EINVAL: nel caso di un errore sconosciuto del server
 *      ENOENT: nel caso in cui il file specificato non esiste nel server
 *      EPERM: nel caso in cui la lock sul file specificato è acquisita da un altro client 
 * Ritorna: 0 in caso di successo, -1 in caso di errore, imposta errno adeguatamente
 */
int unlockFile(const char *pathname);

/*
 * Invia al server una richiesta di rimozione del file con filename == pathname
 * Parametri:
 *      pathname: il percorso assoluto del file su cui rilasciare la lock
 * Errno:
 *      ENOTCONN: se il client non ha una connessione aperta con il server
 *      ENAMETOOLONG: se il percorso specificato ha una lunghezza maggiore di UNIX_PATH_MAX
 *      EINVAL: nel caso di un errore sconosciuto del server
 *      ENOENT: nel caso in cui il file specificato non esiste nel server
 *      EPERM: nel caso in cui la lock sul file specificato è acquisita da un altro client 
 * Ritorna: 0 in caso di successo, -1 in caso di errore, imposta errno adeguatamente
 */
int removeFile(const char *pathname);

void set_p() {
    print_upper_r = 1;
}

void set_dirname(char *dirname) {
    if(sel_dirname == NULL) {
        sel_dirname = malloc((strlen(dirname) + 1) * sizeof(char));
    } else {
        sel_dirname = realloc(sel_dirname, (strlen(dirname) + 1) * sizeof(char));
    }

    strcpy(sel_dirname, dirname);
}

void reset_dirname() {
    if(sel_dirname != NULL) {
        free(sel_dirname);

        sel_dirname = NULL;
    }
}

int save_file(char *file_list) {
    FILE *file;
    char delimiter[2] = {1, '\0'};

    char *pathname;
    char *filename;
    char *content;
    char *saveptr;

    if(sel_dirname == NULL) {
        errno = ENOENT;

        return -1;
    }

    if(strcmp(file_list, "") == 0) {
        return 0;
    }

    filename = strtok_r(file_list, delimiter, &saveptr);

    while(filename != NULL) {
        content = strtok_r(NULL, delimiter, &saveptr);

        pathname = malloc((UNIX_PATH_MAX + strlen(content) + 2) * sizeof(char));

        strcpy(pathname, sel_dirname);
            
        if(sel_dirname[strlen(sel_dirname) - 1] != '/') {
            strcat(pathname, "/");
        }

        filename = strrchr(filename, '/') + 1;

        strcat(pathname, filename);

        if((file = fopen(pathname, "w")) == NULL) {
            return -1;
        }

        fwrite(content, sizeof(char), strlen(content), file);

        free(pathname);

        fclose(file);

        filename = strtok_r(NULL, delimiter, &saveptr);
    }

    return 0;
}

void set_openfile(int n_val, char *pathname) {
    openfile = n_val;

    if(n_val == 0) {
        if(last_request_target != NULL) {
            free(last_request_target);

            last_request_target = NULL;
        }
    } else {
        if(last_request_target == NULL) {
            last_request_target = malloc((strlen(pathname) + 1) * sizeof(char));
        } else {
            last_request_target = realloc(last_request_target, (strlen(pathname) + 1) * sizeof(char));
        }

        strcpy(last_request_target, pathname);
    }
}

int check_openfile(char *pathname) {
    if(openfile == 0) {
        return 0;
    }

    if(strcmp(last_request_target, pathname) == 0) {
        return 1;
    }

    return 0;
}

int send_request(char *type, request_args *args) {
    char *request_m;
    char delimiter[2] = {1, '\0'};

    int request_size;

    if(type == NULL) {
        errno = EINVAL;

        return -1;
    }

    // Genera il messaggio di richiesta
    if(strcmp(type, CLOSECONN) == 0) {
        request_m = malloc(2 * sizeof(char));

        strcpy(request_m, type);
    } else if(strcmp(type, OPENFILE) == 0) {
        if(args == NULL || args->pathname == NULL || args->flags < 0 || args->flags > 3) {
            errno = EINVAL;

            return -1;
        }

        request_m = malloc((UNIX_PATH_MAX + 5) * sizeof(char));

        strcpy(request_m, type);
        strcat(request_m, delimiter);
        strcat(request_m, args->pathname);
        strcat(request_m, delimiter);

        switch (args->flags) {
            case 0:
                strcat(request_m, "0");
                break;
            case 1:
                strcat(request_m, "1");
                break;
            case 2:
                strcat(request_m, "2");
                break;
            case 3:
                strcat(request_m, "3");
                break;

        }
    } else if(strcmp(type, CLOSEFILE) == 0) {
        if(args == NULL || args->pathname == NULL) {
            errno = EINVAL;

            return -1;
        }

        request_m = malloc((UNIX_PATH_MAX + 3) * sizeof(char));

        strcpy(request_m, type);
        strcat(request_m, delimiter);
        strcat(request_m, args->pathname);
    } else if(strcmp(type, WRITEFILE) == 0) {
        if(args == NULL || args->pathname == NULL || args->content == NULL) {
            errno = EINVAL;

            return -1;
        }

        request_m = malloc((4 + UNIX_PATH_MAX + strlen(args->content)) * sizeof(char));

        strcpy(request_m, type);
        strcat(request_m, delimiter);
        strcat(request_m, args->pathname);
        strcat(request_m, delimiter);
        strcat(request_m, args->content);
    } else if(strcmp(type, READFILE) == 0) {
        if(args == NULL || args->pathname == NULL) {
            errno = EINVAL;

            return -1;
        }

        request_m = malloc((UNIX_PATH_MAX + 3) * sizeof(char));

        strcpy(request_m, type);
        strcat(request_m, delimiter);
        strcat(request_m, args->pathname);
    } else if(strcmp(type, READNFILE) == 0) {
        if(args == NULL || args->n == NULL) {
            errno = EINVAL;

            return -1;
        }

        request_m = malloc((strlen(args->n) + 3) * sizeof(char));

        strcpy(request_m, type);
        strcat(request_m, delimiter);
        strcat(request_m, args->n);
    } else if(strcmp(type, APPENDFILE) == 0) {
        if(args == NULL || args->pathname == NULL || args->content == NULL) {
            errno = EINVAL;

            return -1;
        }

        request_m = malloc((4 + UNIX_PATH_MAX + args->size) * sizeof(char));

        strcpy(request_m, type);
        strcat(request_m, delimiter);
        strcat(request_m, args->pathname);
        strcat(request_m, delimiter);
        strcat(request_m, args->content);
    } else if(strcmp(type, LOCKFILE) == 0) {
        if(args == NULL || args->pathname == NULL) {
            errno = EINVAL;

            return -1;
        }

        request_m = malloc((strlen(args->pathname) + 3) * sizeof(char));

        strcpy(request_m, type);
        strcat(request_m, delimiter);
        strcat(request_m, args->pathname);
    } else if(strcmp(type, UNLOCKFILE) == 0) {
        if(args == NULL || args->pathname == NULL) {
            errno = EINVAL;

            return -1;
        }

        request_m = malloc((strlen(args->pathname) + 3) * sizeof(char));

        strcpy(request_m, type);
        strcat(request_m, delimiter);
        strcat(request_m, args->pathname);
    } else if(strcmp(type, REMOVEFILE) == 0) {
        if(args == NULL || args->pathname == NULL) {
            errno = EINVAL;

            return -1;
        }

        request_m = malloc((strlen(args->pathname) + 3) * sizeof(char));

        strcpy(request_m, type);
        strcat(request_m, delimiter);
        strcat(request_m, args->pathname);
    } else if(strcmp(type, WRITE_NO_CONTENT) == 0) {
        request_m = malloc(3 * sizeof(char));

        strcpy(request_m, WRITE_NO_CONTENT);
    }

    request_size = strlen(request_m) + 1;

    if(write(socket_fd, &request_size, sizeof(request_size)) == -1) {
        return -1;
    }

    if(write(socket_fd, request_m, request_size) == -1) {
        return -1;
    }

    free(request_m);

    return 0;
}

int manage_response(char *type, response_args *args) {
    char *response_m;
    char *response_code;
    char *filename;
    char *content;
    char *saveptr;
    char *temp;
    char delimiter[2] = {1, '\0'};

    int response_size;
    int result;

    if(read(socket_fd, &response_size, sizeof(int)) == -1) {
        return -1;
    }

    response_m = malloc(response_size * sizeof(char));
    memset(response_m, 0, response_size);

    if(read(socket_fd, response_m, response_size) == -1) {
        return -1;
    }

    temp = malloc(response_size * sizeof(char));
    strcpy(temp, response_m);

    response_code = strtok_r(temp, delimiter, &saveptr);
    // L'operazione ha avuto successo e la risposta viene elaborata in base al tipo della richiesta
    if(strcmp(response_code, SUCCESS) == 0) {
        if(strcmp(type, OPENFILE) == 0) {
            if(response_size > 2) {
                if(sel_dirname != NULL) {
                    save_file(response_m + 2);
                }
            }
        } else if(strcmp(type, WRITEFILE) == 0) {
            if(response_size > 2) {
                if(sel_dirname != NULL) {
                    save_file(response_m + 2);
                }
            }
        } else if(strcmp(type, READFILE) == 0) {
            if(response_size > 2) {
                char *file_content;
                char *t_content;

                if(args == NULL) {
                    errno = EINVAL;

                    return -1;
                }

                t_content = strtok_r(NULL, delimiter, &saveptr);

                file_content = malloc((strlen(t_content) + 1) * sizeof(char));

                strcpy(file_content, t_content);
        
                *args->buf = file_content;
                *args->size = strlen(file_content);
            }
        } else if(strcmp(type, READNFILE) == 0) {
            if(response_size > 2) {
                if(print_upper_r) {
                    filename = strtok_r(NULL, delimiter, &saveptr);
                    while(filename != NULL) {
                        content = strtok_r(NULL, delimiter, &saveptr);

                        printf("-R: Successo, letto il file %s, di %ldbytes\n", filename, strlen(content));

                        filename = strtok_r(NULL, delimiter, &saveptr);;
                    }
                }

                if(sel_dirname != NULL) {
                    save_file(response_m + 2);
                }
            } else {
                if(print_upper_r) {
                    printf("-R: Successo, il server non contiene alcun file\n");
                }
            }
        } else if(strcmp(type, APPENDFILE) == 0) {
            if(response_size > 2) {
                if(sel_dirname != NULL) {
                    save_file(response_m + 2);
                }
            }             
        } 

        result = 0;
    }

    // Verifica se è avvenuto un errore e imposta errno
    if(strcmp(response_code, ALREADY_OPENED) == 0) {
        errno = EBADR;

        result = -1;
    } else if(strcmp(response_code, FILE_NOT_EXIST) == 0) {
        errno = ENOENT;

        result = -1;
    } else if(strcmp(response_code, UNKNOWN) == 0) {
        errno = EINVAL;

        result = -1;
    } else if(strcmp(response_code, FILENAME_TOO_LONG) == 0) {
        errno = ENAMETOOLONG;

        result = -1;
    } else if(strcmp(response_code, FILE_ALREADY_EXIST) == 0) {
        errno = EEXIST;

        result = -1;
    } else if(strcmp(response_code, FILE_NOT_OPENED) == 0) {
        errno = EBADF;

        result = -1;
    } else if(strcmp(response_code, FILE_LOCKED) == 0) {
        errno = EPERM;

        result = -1;
    } else if(strcmp(response_code, NOT_ENO_MEM) == 0) {
        errno = ENOMEM;

        result = -1;
    }

    free(temp);
    free(response_m);

    return result;
}

int openConnection(const char *socketname, int msec, const struct timespec abstime) {
    struct timespec acttime;
    struct sockaddr_un sock_addr;

    int opened = 0;

    // Verifica se una connessione è già aperta
    if(sel_socketname != NULL) {
        errno = EISCONN;

        return -1;
    }

    // Inizializza il socket
    socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    sock_addr.sun_family = AF_UNIX;
    strncpy(sock_addr.sun_path, socketname, UNIX_PATH_MAX);

    // Tenta di aprire una connessione con il server fino al raggiungimento di abstime
    acttime.tv_sec = time(NULL);
    while(!opened && acttime.tv_sec < abstime.tv_sec) {
        if(connect(socket_fd, (struct sockaddr*) &sock_addr, sizeof(sock_addr)) == -1) {
            if(usleep(msec * 1000)) {
                return -1;
            }
        } else {
            opened = 1;
        }

        acttime.tv_sec = time(NULL);
    }

    //Verifica se la connessione è stata aperta con successo
    if(!opened) {
        //Si è verificato un problema
        errno = ETIMEDOUT;

        free(sel_socketname);

        sel_socketname = NULL;

        return -1;
    }

    // La connessione è stata aperta con successo

    sel_socketname = malloc((strlen(socketname) + 1) * sizeof(char));

    strcpy(sel_socketname, socketname);

    return 0;
}

int closeConnection(const char *socketname) {
    // Verifica se la connessione non è già stata chiusa
    if(sel_socketname == NULL) {
        errno = ENOTCONN;

        return -1;
    }

    // Verifica se il sockname passato come argomento è lo stesso di quello su cui è aperta la connessione
    if(strcmp(socketname, sel_socketname) != 0) {
        errno = EINVAL;

        return -1;
    }

    if(send_request(CLOSECONN, NULL) != 0) {
        return -1;
    }

    manage_response(CLOSECONN, NULL);

    // Chiude la connessione
    if(close(socket_fd)) {
        return -1;
    }

    free(sel_socketname);

    sel_socketname = NULL;
    socket_fd = -1;

    return 0;
}

int openFile(const char *pathname, int flags) {
    request_args args;

    int result;

    // Verifica se la connessione con il server è stata effettuata
    if(sel_socketname == NULL) {
        errno = ENOTCONN;

        return -1;
    }

    args.pathname = (char *)pathname;
    args.flags = flags;

    if(send_request(OPENFILE, &args) != 0) {
        return -1;
    }

    result = manage_response(OPENFILE, NULL);

    if(flags == 3) {
        set_openfile(1, (char *)pathname);
    } else {
        set_openfile(0, NULL);
    }

    return result;
}

int closeFile(const char *pathname) {
    request_args args;

    int result;

    // Verifica se la connessione con il server è stata effettuata
    if(sel_socketname == NULL) {
        errno = ENOTCONN;

        set_openfile(0, NULL);

        return -1;
    }

    args.pathname = (char *)pathname;
    if(send_request(CLOSEFILE, &args) != 0) {
        set_openfile(0, NULL);

        return -1;
    }

    result = manage_response(CLOSEFILE, NULL);

    set_openfile(0, NULL);

    return result;
}

int readFile(const char *pathname, void **buf, size_t *size) {
    request_args request_args;
    response_args response_args;

    int result;

    // Verifica se la connessione con il server è stata effettuata
    if(sel_socketname == NULL) {
        errno = ENOTCONN;

        set_openfile(0, NULL);

        return -1;
    }

    if(strlen(pathname) > UNIX_PATH_MAX) {
        errno = ENAMETOOLONG;

        set_openfile(0, NULL);

        return -1;
    }

    request_args.pathname = (char *)pathname;
    if(send_request(READFILE, &request_args) != 0) {
        set_openfile(0, NULL);

        return -1;
    }

    response_args.buf = buf;
    response_args.size = size;
    result = manage_response(READFILE, &response_args);

    set_openfile(0, NULL);

    return result;
}

int readNFiles(int n, const char *dirname) {
    request_args args;
    char *string_n;

    int result;

    // Verifica se la connessione con il server è stata effettuata
    if(sel_socketname == NULL) {
        errno = ENOTCONN;

        return -1;
    }

    if(dirname != NULL && strlen(dirname) > UNIX_PATH_MAX) {
        errno = ENAMETOOLONG;

        return -1;
    }

    // Verifica se la directory dirname esiste e se l'utente ha i permessi di leggere scrivere in quella directory
    if(dirname != NULL) {
        if(access(dirname, F_OK) == -1) {
            errno = ENOENT;

            return -1;
        } else if(access(dirname, R_OK | W_OK) == -1) {
            errno = EPERM;

            return -1;
        }
    }

    if(n != 0) {
        string_n = malloc((int)((ceil(log10(n)) + 2) * sizeof(char)));
    } else {
        string_n = malloc(2 * sizeof(char));
    }

    sprintf(string_n, "%d", n);

    args.n = string_n;
    if(send_request(READNFILE, &args) == -1) {
        set_openfile(0, NULL);

        return -1;
    }

    free(string_n);

    if(dirname != NULL) {   
        set_dirname((char *)dirname);
    }

    result = manage_response(READNFILE, NULL);

    set_openfile(0, NULL);

    reset_dirname();

    return result;
}

int writeFile(const char *pathname, const char *dirname) {
    FILE *file;
    long file_size;
    char *file_content;

    request_args args;

    int result = 0;

    // Verifica se la connessione con il server è stata effettuata
    if(sel_socketname == NULL) {
        errno = ENOTCONN;

        return -1;
    }

    if(check_openfile((char *)pathname) == 0) {
        errno = EBADF;

        return -1;
    }

    // Verifica se la directory dirname esiste e se l'utente ha i permessi di leggere scrivere in quella directory
    if(dirname != NULL) {
        if(access(dirname, F_OK) == -1) {
            errno = EPERM;

            return -1;
        } else if(access(dirname, R_OK | W_OK) == -1) {
            errno = EPERM;

            return -1;
        }
    }

    // Verifica e apre il file pathname
    if((file = fopen(pathname, "rb")) == NULL) {
        errno = ENOENT;

        return -1;
    }

    fseek(file, 0L, SEEK_END);
    file_size = ftell(file);
    fseek(file, 0L, SEEK_SET);

    file_content = malloc((file_size + 1) * sizeof(char));
    memset(file_content, 0, file_size + 1);
    fread(file_content, sizeof(char), file_size, file);
    fclose(file);

    if(strcmp(file_content, "") != 0) {
        args.pathname = (char *)pathname;
        args.content = file_content;
        send_request(WRITEFILE, &args);

        if(dirname != NULL) {
            set_dirname((char *)dirname);
        }

        result = manage_response(WRITEFILE, NULL);
    } else {
        send_request(WRITE_NO_CONTENT, NULL);

        manage_response(WRITE_NO_CONTENT, NULL);
    }

    free(file_content);

    set_openfile(0, NULL);

    return result;
}

int appendToFile(const char *pathname, void *buf, size_t size, const char *dirname) {
    request_args args;

    int result;

    // Verifica se la connessione con il server è stata effettuata
    if(sel_socketname == NULL) {
        errno = ENOTCONN;

        return -1;
    }

    // Verifica se la directory dirname esiste e se l'utente ha i permessi di leggere scrivere in quella directory
    if(dirname != NULL) {
        if(access(dirname, F_OK | R_OK | W_OK) == -1) {

            return -1;
        }
    }

    args.pathname = (char *)pathname;
    args.content = (char *)buf;
    args.size = size;
    send_request(APPENDFILE, &args);

    if(dirname != NULL) {
        set_dirname((char *)dirname);
    }
    result = manage_response(APPENDFILE, NULL);

    set_openfile(0, NULL);

    return result;
}

int lockFile(const char *pathname) {
    request_args args;

    int result;

    // Verifica se la connessione con il server è stata effettuata
    if(sel_socketname == NULL) {
        errno = ENOTCONN;

        return -1;
    }

    if(strlen(pathname) > UNIX_PATH_MAX) {
        errno = ENAMETOOLONG;

        return -1;
    }

    args.pathname = (char *)pathname;
    send_request(LOCKFILE, &args);

    result = manage_response(LOCKFILE, NULL);

    set_openfile(0, NULL);

    return result;
}

int unlockFile(const char *pathname) {
    request_args args;

    int result;

    // Verifica se la connessione con il server è stata effettuata
    if(sel_socketname == NULL) {
        errno = ENOTCONN;

        return -1;
    }

    if(strlen(pathname) > UNIX_PATH_MAX) {
        errno = ENAMETOOLONG;

        return -1;
    }

    args.pathname = (char *)pathname;
    send_request(UNLOCKFILE, &args);

    result = manage_response(UNLOCKFILE, NULL);

    set_openfile(0, NULL);

    return result;
}

int removeFile(const char *pathname) {
    request_args args;

    int result;

    // Verifica se la connessione con il server è stata effettuata
    if(sel_socketname == NULL) {
        errno = ENOTCONN;

        return -1;
    }

    if(strlen(pathname) > UNIX_PATH_MAX) {
        errno = ENAMETOOLONG;

        return -1;
    }

    args.pathname = (char *)pathname;
    send_request(REMOVEFILE, &args);

    result = manage_response(REMOVEFILE, NULL);

    set_openfile(0, NULL);

    return result;
}
