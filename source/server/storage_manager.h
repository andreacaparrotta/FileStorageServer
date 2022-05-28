#include <time.h>

#include "definitions.h"
#include "ht_manager.h"

#define UNIX_PATH_MAX 108

struct statistics {
    long max_stored_bytes;                                  // Il numero massimo di byte memorizzati nello storage
    int max_stored_files;                                   // Il numero massimo di file memorizzati nello storage
    int replaced_files;                                     // il numero di file rimpiazzati
};

struct size {
    long size_bytes;                                        // Dimensione massima dello storage in byte
    int size_n;                                             // Numero massimo di file che possono essere contenuti nello storage

    int size_ht;                                            // Dimensione dell'array che modella l'hash table

    long occupied_bytes;                                    // Byte occupati nello storage
    int occupied_size_n;                                    // Numero di file presenti nello storage
};

struct storage{
    struct f_el **ht;                                       // Array di puntatori a file che modella l'hash tabel
    struct size size;                                       // Struct contenente tutte le dimensioni dello storage
    struct statistics statistics;                           // Struct contenente tutte le statistiche dello storage
    char *log_filename;                                     // Il filename del file di log
};

typedef struct storage storage;

pthread_mutex_t lock_storage = PTHREAD_MUTEX_INITIALIZER;

// Interfacce funzioni di supporto
/*
 * Crea un nuovo file e lo inserisce nella hash table
 * Parametri:
 *      storage: lo storage in cui inserire il nuovo file
 *      filename: il filename da associare al file
 *      max: il numero massimo di connessioni contemporaneamente attive
 * Errno:
 *      EINVAL: se storage == NULL oppure filename == NULL oppure se max < 0
 *      EEXIST: se esiste già un file con il filename specificato
 * Ritorna: il puntatore ad un eventuale file vittima, oppure NULL in caso di errore o nel caso non ci sia alcuna vittima
 *          è necessario verificare errno per distinguere i due casi
 */
f_el *create_file(storage *storage, char *filename, int max);

/*
 * Elimina un file dallo storage
 * Parametri:
 *      storage: lo storage da cui eliminare il file
 *      victim: il file che deve essere eliminato
 * Errno:
 *      EINVAL: se storage == NULL oppure victim == NULL
 *      ENOENT: se il file specificato non esiste nello storage
 * Ritorna: 0 in caso di successo, -1 in caso di errore
 */
int delete_file(storage *storage, f_el *victim);

/*
 * Verifica se il file è stato aperto dal client connesso attraverso il socket descritto da socket_fd
 * Parametri:
 *      file: il file su cui eseguire la verifica
 *      socket_fd: il descrittore del socket su cui eseguire la verifica
 *      max: il numero massimo di connessioni contemporaneamente attive
 * Errno:
 *      EINVAL: se file == NULL oppure socket_fd < 0 oppure max < 0
 * Ritorna: 0 se il file non è stato aperto da quel client, 1 in caso contrario
 */
int check_opened(f_el *file, int socket_fd, int max);

/*
 * Apre il file inserendo il "socket_fd" nell'array "opened" del file
 * Parametri:
 *      storage: lo storage che contiene il file da aprire
 *      file: il file da aprire
 *      socket_fd: il descrittore del socket su cui è stata ricevuta la richiesta di apertura
 *      max: il numero massimo di connessioni contemporaneamente attive
 *      log: 1 se bisogna scrivere nel file di lock, 0 altrimenti
 * Errno:
 *      EINVAL: se storage == NULL oppure file == NULL oppure socket_fd == NULL oppure max < 0
 * Ritorna: 1 in caso di successo, 0 in caso di errore
 */
int open_file(storage *storage, f_el *file, int socket_fd, int max, int log);

/*
 * Chiude il file rimuovendo il "socket_fd" dall'array "opened" del file
 * Parametri:
 *      storage: lo storage che contiente il file da chiudere
 *      file: il file da chiudere
 *      socket_fd: il descrittore del socket su cui è stata ricevuta la richiesta di chiusura
 *      max: il numero massimo di connessioni contemporaneamente attive
 * Errno:
 *      EINVAL: se storage == NULL oppure file == NULL oppure socket_fd == NULL oppure max < 0
 * Ritorna: 0 in caso di successo, -1 in caso di errore
 */
int close_file(storage *storage, f_el *file, int socket_fd, int max);

/*
 * Verifica se la lock sul file è ottenuta dal client connesso al server con il socket con descrittore "socket_fd"
 * Parametri:
 *      file: il file su cui eseguire la verifica
 *      socket_fd: il descrittore del socket su cui eseguire la verifica
 * Errno:
 *      EINVAL: se file == NULL oppure socket_fd < 0
 * Ritorna: 0 se la lock non è posseduta da nessuno, 1 se la lock è posseduta da chi ha richiesto l'operazione, -1 se la lock è posseduta da un altro client
 */
int check_locked(f_el *file, int socket_fd);

/*
 * Imposta lo stato di lock sul file
 * Parametri:
 *      storage: lo storage che contiene il file su cui impostare la lock
 *      file: il file su cui impostare la lock
 *      socket_fd: il descrittore del socket da cui è stata ricevuta la richiesta di lock
 *      lock_type: 0 se la lock è stata richiesta esplicitamente con comando lockfile, 1 se la lock è stata richiesta con flag O_LOCK
 * Errno:
 *      EINVAL: se storage == NULL oppure file == NULL oppure socket_fd < 0
 *      EPERM: se la lock sul file è già posseduta da un altro utente
 * Ritorna: 0 in caso di successo, -1 in caso di errore
 */
int lock_file(storage *storage, f_el *file, int socket_fd, int lock_type);

/*
 * Rilascia la lock sul file
 * Parametri:
 *      storage: lo storage che contiene il file su cui rilasciare la lock
 *      file: il file su cui rilasciare la lock
 *      socket_fd: il descrittore del socket da cui è stata ricevuta la richiesta di unlock
 * Errno:
 *      EINVAL: se storage == NULL oppure file == NULL oppure socket_fd < 0
 *      EPERM: se la lock non è posseduta dall'utente che ha richiesto l'operazione
 * Ritorna: 0 in caso di successo, -1 in caso di errore
 */
int unlock_file(storage *storage, f_el *file, int socket_fd);

/*
 * Rimpiazza n file fino a svuotare un quantitativo di memoria >= required_space
 * Parametri:
 *      storage: lo storage in cui liberare la memoria
 *      required_space: il quantitativo di memoria richiesto
 *      exonerated: eventuale file da non considerare come file vittima
 * Errno:
 *      EINVAL: se storage == NULL oppure required_space <= 0 oppure storage->ht == NULL
 *      ENOMEM: se required_space è maggiore della dimensione massima dello storage
 * Ritorna: un array contenente i file scelti come vittima, NULL in caso di errore 
 */
f_el *replace_files(storage *storage, long required_space, char *exonerated);

/*
 * Rimpiazza un file
 * Parametri:
 *      storage: lo storage in cui rimpiazzare il file
 * Errno:
 *      EINVAL: se storage == NULL oppure storage->ht == NULL
 *      EPERM: se non c'è un file da selezionare come vittima nello storage
 * Ritorna: il puntatore al file scelto come vittima, NULL in caso di errore
 */
f_el *replace_file(storage *storage);

/*
 * Calcola la dimensione della stringa contenente i filename e contenuti dei file letti
 * Parametri:
 *      ht: l'hash table in cui cercare i file
 *      size: la dimensione dell'array che modella l'hash table
 *      n: il numero di file da leggere
 *      socket_fd: il descrittore del socket su cui è stata ottenuta la richiesta
 * Errno:
 *      EINVAL: se ht == NULL oppure size <= 0 opppure n < 0 oppure socket_fd < 0
 * Ritorna: la dimesione della stringa contenente i filename e contenuti dei file letti in caso di successo, -1 in caso di errore
 */
int read_n_files_size(f_el **ht, int size, int n, int socket_fd);

/*
 * Genera la stringa contenente i filename e contenuti dei file letti
 * Parametri:
 *      ht: l'hash table in cui cercare i file
 *      size: la dimensione dell'array che modella l'hash table
 *      n: il numero di file da leggere
 *      result: la stringa che conterrà il risultato della funzione
 *      log_file: il file di log
 *      socket_fd: il descrittore del socket su cui è stata ottenuta la richiesta
 * Errno:
 *      EINVAL: se ht == NULL oppure size <= 0 oppure n < 0 oppure log_file == NULL oppure socket_fd < 0
 * Ritorna: 0 in caso di successo, -1 in caso di errore
 */
int set_read_n_files(f_el **ht, int size, int n, char *result, FILE *log_file, int socket_fd);

// Interfacce funzioni api
/*
 * Gestisce l'apertura di un file
 * Parametri:
 *      storage: lo storage in cui cercare il file da aprire, oppure in cui creare il file nel caso in cui non esistesse e il flag O_CREATE fosse impostato
 *      filename: il filename del file da aprire o da creare nel caso in cui non esistesse
 *      flags: contiene i flags richiesti per l'apertura del file, per la specifica dei possibili flag vedi "definitions.h"
 *      socket_fd: il descrittore del socket su cui è stata ottenuta la richiesta
 *      max: il numero massimo di connessioni contemporaneamente attive
 * Errno:
 *      EINVAL: se storage == NULL oppure storage->ht == NULL || filename == NULL oppure socket_fd < 0 oppure max <= 0
 *      ENAMETOOLONG: se filename ha una lunghezza maggiore di UNIX_PATH_MAX
 *      ENOENT: se non esiste un file con il filename specificato e il flag O_CREATE non è specificato
 *      EEXIST: se esiste un file con filename specificato e il flag O_CREATE è specificato
 *      EBADR: se il file è stato già aperto dall'utente che ha richiesto l'operazione
 *      EPERM: se il flag O_LOCK è impostato e la lock è posseduta da un altro utente
 * Ritorna: il puntatore ad un eventuale file espulso durante la creazione di un nuovo file, NULL in caso di successo senza espulsione di file oppure in caso di errore, verificare errno per scoprire eventuali errori
 */
f_el *openFile(storage *storage, char *filename, int flags, int socket_fd, int max);

/*
 * Gestisce la chiusura di un file
 * Parametri:
 *      storage: lo storage in cui cercare il file da chiudere
 *      filename: il filename del file da chiudere
 *      socket_fd: il descrittore del socket su cui è stata ottenuta la richiesta
 *      max: il numero massimo di connessioni contemporaneamente attive
 * Errno:
 *      EINVAL: se storage == NULL oppure storage->ht == NULL oppure filename == NULL oppure socket_fd < 0 oppure max <= 0
 *      ENAMETOOLONG: se filename ha una lunghezza maggiore di UNIX_PATH_MAX
 *      ENOENT: se non esiste un file con il filename specificato
 * Ritorna: 0 in caso di successo, -1 in caso di errore
 */
int closeFile(storage *storage, char *filename, int socket_fd, int max);

/*
 * Scrive il contenuto di un file
 * Parametri:
 *      storage: lo storage in cui cercare il file in cui scrivere il contenuto
 *      filename: il filename del file da scrivere
 *      socket_fd: il descrittore del socket su cui è stata ottenuta la richiesta
 *      content: il contenuto per il file
 *      max: il numero massimo di connessioni contemporaneamente attive
 * Errno:
 *      EINVAL: se storage == NULL oppure storage->ht == NULL oppure filename == NULL oppure socket_fd < 0 oppure content == NULL oppure max <= 0
 *      ENAMETOOLONG: se filename ha una lunghezza maggiore di UNIX_PATH_MAX
 *      ENOENT: se non esiste un file con il filename specificato
 *      EPERM: se un altro utente possiede la lock sul file
 *      EBADF: se il file non è stato aperto dal client che ha richiesto l'operazione
 *      ENOMEM: se il contenuto ha dimensione superiore alla capacità massima dello storage
 * Ritorna: un array contenente eventuali file espulsi per fare spazio nello storage oppure NULL in caso di successo, NULL in caso di errore, verificare errno per scoprire eventuali errori
 */
f_el *writeFile(storage *storage, char *filename, int socket_fd, char *content, int max);

/*
 * Legge il contenuto del file con filename specificato
 * Parametri:
 *      storage: lo storage in cui cercare il file da leggere
 *      filename: il filename del file da leggere
 *      socket_fd: il descrittore del socket su cui è stata ottenuta la richiesta
 * Errno:
 *      EINVAL: se storage == NULL oppure storage->ht == NULL oppure filename == NULL oppure socket_f < 0
 *      ENAMETOOLONG: se filename ha una lunghezza maggiore di UNIX_PATH_MAX
 *      ENOENT: se non esiste un file con il filename specificato
 *      EPERM: se un altro utente possiede la lock sul file
 * Ritorna: la stringa contenente il contenuto del file in caso di successo, NULL in caso di errore
 */
char *readFile(storage *storage, char *filename, int socket_fd);

/*
 * Legge n file qualsiasi contenuti nello storage, se n == 0 allora vengono letti tutti i file 
 * Parametri:
 *      storage: lo storage da cui leggere i file
 *      n: il numero di file da leggere
 *      socket_fd: il descrittore del socket su cui è stata ottenuta la richiesta
 * Errno:
 *      EINVAL: se storage == NULL oppure storage->ht == NULL oppure n < 0 oppure socket_fd < 0
 * Ritorna: la stringa contenente i filename e contenuti di tutti i file letti in caso di successo, NULL in caso di errore
 */
char *readNFiles(storage *storage, int n, int socket_fd);

/*
 * Concatena "content" al contenuto del file specificato
 * Parametri:
 *      storage: lo storage in cui cercare il file
 *      filename: il filename del file a cui concatenare il contenuto
 *      socket_fd: il descrittore del socket su cui è stata ottenuta la richiesta
 *      content: il contenuto da concatenare
 *      max: il numero massimo di connessioni attive contemporaneamente
 * Errno:
 *      EINVAL: se storage == NULL oppure storage->ht == NULL oppure filename == NULL oppure socket_fd < 0 oppure content == NULL oppure max <= 0
 *      ENAMETOOLONG: se filename ha una lunghezza maggiore di UNIX_PATH_MAX
 *      ENOENT: se non esiste un file con il filename specificato
 *      EPERM: se un altro utente possiede la lock sul file
 *      EBADF: se il file non è stato aperto dall'utente che ha richiesto l'operazione
 *      ENOMEM: se la dimensione del vecchio contenuto aggiunta alla dimensione del nuovo contenuto supera la capacità massima dello storage
 * Ritorna: un array di file vittima espulsi per fare spazio nello storage oppure NULL in caso di successo, NULL in caso di errore, verificare errno per scoprire eventuali errori
 */
f_el *appendToFile(storage *storage, char *filename, int socket_fd, char *content, int max);

/*
 * Imposta la lock su un file
 * Parametri:
 *      storage: lo storage in cui cercare il file
 *      filename: il filename del file su cui impostare la lock
 *      socket_fd: il descrittore del socket su cui è stata ottenuta la richiesta
 * Errno:
 *      EINVAL: se storage == NULL oppure storage->ht == NULL oppure filename == NULL oppure socket_fd < 0
 *      ENOENT: se non esiste un file con il filename specificato
 *      EPERM: se un altro utente possiede la lock sul file
 * Ritorna: 0 in caso di successo, -1 in caso di errore
 */
int lockFile(storage *storage, char *filename, int socket_fd);

/*
 * Rilascia la lock su un file
 * Parametri:
 *      storage: lo storage in cui cercare il file
 *      filename: il filename del file su cui rilasciare la lock
 *      socket_fd: il descrittore del socket su cui è stata ottenuta la richiesta
 * Errno:
 *      EINVAL: se storage == NULL oppure storage->ht == NULL oppure filename == NULL oppure socket_fd < 0
 *      ENOENT: se non esiste un file con il filename specificato
 *      EPERM: se un altro utente possiede la lock sul file
 * Ritorna: 0 in caso di successo, -1 in caso di errore
 */
int unlockFile(storage *storage, char *filename, int socket_fd);

/*
 * Rimuove un file dallo storage, richiede che l'utente possieda la lock sul file da eliminare
 * Parametri:
 *      storage: lo storage in cui cercare il file
 *      filename: il filename del file da eliminare
 *      socket_fd: il descrittore del socket su cui è stata ottenuta la richiesta
 * Errno:
 *      EINVAL: se storage == NULL oppure storage->ht == NULL oppure filename == NULL oppure socket_fd < 0
 *      ENOENT: se non esiste un file con il filename specificato
 *      EPERM: se un altro utente possiede la lock sul file
 * Ritorna: 0 in caso di successo, -1 in caso di errore
 */
int removeFile(storage *storage, char *filename, int socket_fd);

/*
 * Ripristina lo storage in uno stato coerente al momento della chiusura della connessione, rilasciando eventuali lock possedute dalla connessione chiusa e chiudendo file
 * Parametri:
 *      storage: lo storage in cui cercare il file
 *      socket_fd: il descrittore del socket su cui è stata ottenuta la richiesta
 *      max: il numero massimo di connessioni contemporaneamente aperte
 * Errno:
 *      EINVAL: se storage == NULL oppure storage->ht == NULL oppure socket_fd < 0 oppure max <= 0
 * Ritorna: 0 in caso di successo, -1 in caso di errore
 */
int clean_closed_conn(storage *storage, int socket_fd, int max);

/*
 * Inserisce l'informazione di log nel caso della scrittura di un file vuoto
 * Parametri:
 *      storage: lo storage per ottenere il filename del file di log
 * Ritorna: none
 */
void write_no_content(storage *storage);

/*
 * Restituisce una stringa formattata che rappresenta la data e ora attuale
 * Parametri: none
 * Ritorna: una stringa che rappresenta la data e ora attuale
 */
char *get_timestamp();

/*
 * Funzioni di supporto per l'api 
 */
f_el *create_file(storage *storage, char *filename, int max) {
    f_el **ht;
    f_el *file;

    f_el *victim = NULL;

    struct timespec time;

    int i;

    if(storage == NULL || filename == NULL || max <= 0) {
        errno = EINVAL;

        return NULL;
    }

    ht = storage->ht;

    if(lookup(ht, storage->size.size_ht, filename) != NULL) {
        errno = EEXIST;

        return NULL;
    }

    file = malloc(sizeof(f_el));

    strcpy(file->metadata.filename, filename);
    file->metadata.size = 0;
    clock_gettime(CLOCK_REALTIME, &time);
    file->metadata.last_used = (long long int)time.tv_sec * 1000000000L + (long long int)time.tv_nsec;
    file->metadata.acquired_by = -1;
    file->metadata.lock_type = 0;
    file->metadata.opened = malloc(max * sizeof(int));
    file->metadata.next_file = NULL;
    file->metadata.prev_file = NULL;

    file->data = NULL;
    
    for(i = 0; i < max; i++) {
        file->metadata.opened[i] = -1;
    }

    if(storage->size.size_n == storage->size.occupied_size_n) {
        printf("WORKER: È necessario il rimpiazzamento di un file\n");

        victim = replace_file(storage);
    }

    if(insert(ht, storage->size.size_ht, file) == -1) {
        return NULL;
    }

    storage->size.occupied_size_n += 1;

    if(storage->size.occupied_size_n > storage->statistics.max_stored_files) {
        storage->statistics.max_stored_files = storage->size.occupied_size_n;
    }

    return victim;
}

int delete_file(storage *storage, f_el *victim) {
    f_el **ht;

    int file_size;

    if(storage == NULL || victim == NULL) {
        errno = EINVAL;

        return -1;
    }

    ht = storage->ht;

    file_size = victim->metadata.size;

    if(delete(ht, storage->size.size_ht, victim) == -1) {
        return -1;
    }

    storage->size.occupied_bytes -= file_size;
    storage->size.occupied_size_n -= 1;

    return 0;
}

int check_opened(f_el *file, int socket_fd, int max) {
    int i;

    if(file == NULL || socket_fd < 0 || max <= 0) {
        errno = EINVAL;

        return -1;
    }

    for(i = 0; i < max; i++) {
        if(file->metadata.opened[i] == socket_fd) {
            return 1;
        }
    }

    return 0;
}

int open_file(storage *storage, f_el *file, int socket_fd, int max, int log) {
    FILE *log_file;

    int opened = 0;
    int i;

    if(storage == NULL || file == NULL || socket_fd < 0 || max <= 0) {
        errno = EINVAL;

        return -1;
    }

    for(i = 0; i < max; i++) {
        if(file->metadata.opened[i] == -1) {
            file->metadata.opened[i] = socket_fd;

            if(log) {
                log_file = fopen(storage->log_filename, "a");
                fprintf(log_file, "openfile %s [%s]\n", file->metadata.filename, get_timestamp());
                fclose(log_file);
            }

            opened = 1;

            break;
        }
    }

    return opened;
}

int close_file(storage *storage, f_el *file, int socket_fd, int max) {
    FILE *log_file;

    int i;

    if(storage == NULL || file == NULL || socket_fd < 0 || max <= 0) {
        errno = EINVAL;

        return -1;
    }

    for(i = 0; i < max; i++) {
        if(file->metadata.opened[i] == socket_fd) {
            file->metadata.opened[i] = -1;

            break;
        }
    }
    
    if(check_locked(file, socket_fd) == 1 && file->metadata.lock_type == 1) {
        unlock_file(storage, file, socket_fd);
    }

    log_file = fopen(storage->log_filename, "a");
    fprintf(log_file, "closefile:%s [%s]\n", file->metadata.filename, get_timestamp());
    fclose(log_file);

    return 0;
}

int check_locked(f_el *file, int socket_fd) {
    if(file == NULL || socket_fd < 0) {
        errno = EINVAL;

        return -1;
    }

    if(file->metadata.acquired_by == -1) {
        // La lock sul file non è posseduta da nessuno
        return 0;
    } else if(file->metadata.acquired_by == socket_fd){
        // La lock sul file è posseduta da chi ha richiesto l'operazione
        return 1;
    } else {
        // La lock sul file è posseduta da un altro utente
        return -1;
    }
}

int lock_file(storage *storage, f_el *file, int socket_fd, int lock_type) {
    FILE *log_file;

    if(storage == NULL || file == NULL || socket_fd < 0) {
        errno = EINVAL;

        return -1;
    }

    // Verifica se il file è già locked
    if(check_locked(file, socket_fd) == -1) {
        // Il file è locked
        errno = EPERM;

        return -1;
    }

    // Il file non è locked oppure la richiesta di lock è proveniente dallo stesso processo che possiede già la lock
    file->metadata.acquired_by = socket_fd;
    if(file->metadata.acquired_by == -1) {
        file->metadata.lock_type = lock_type;
    }
    file->metadata.lock_type = lock_type;

    log_file = fopen(storage->log_filename, "a");
    switch(lock_type) {
        case 0:
            fprintf(log_file, "lockfile:%s [%s]\n", file->metadata.filename, get_timestamp());

            break;
        case 1:
            fprintf(log_file, "openlock:%s [%s]\n", file->metadata.filename, get_timestamp());

            break;
    }
    fclose(log_file);

    return 0;
}

int unlock_file(storage *storage, f_el *file, int socket_fd) {
    FILE *log_file;

    if(storage == NULL || file == NULL || socket_fd < 0) {
        errno = EINVAL;

        return -1;
    }

    // Verifica se il file è già locked
    if(check_locked(file, socket_fd) != 1) {
        // Il file non è locked
        errno = EPERM;

        return -1;
    }

    // Il file è locked e la lock è posseduta da chi ha richiesto l'operazione 
    file->metadata.acquired_by = -1;

    log_file = fopen(storage->log_filename, "a");
    fprintf(log_file, "unlockfile:%s [%s]\n", file->metadata.filename, get_timestamp());
    fclose(log_file);

    return 0;
}

f_el *replace_files(storage *storage, long required_space, char *exonerated) {
    FILE *log_file;

    f_el **ht;
    f_el *victim;
    f_el *victims;

    int i;
    int result_size;
    int free_bytes;

    if(storage == NULL || required_space <= 0) {
        errno = EINVAL;

        return NULL;
    }

    if(storage->ht == NULL) {
        errno = EINVAL;

        return NULL;
    }

    ht = storage->ht;

    if(required_space > storage->size.size_bytes) {
        errno = ENOMEM;

        return NULL;
    }

    result_size = 0;
    free_bytes = 0;
    while(required_space > free_bytes) {
        victim = select_victim(ht, storage->size.size_ht, exonerated);

        printf("WORKER: il file %s, di dimensione %dbytes, verrà rimpiazzato\n", victim->metadata.filename, victim->metadata.size);

        log_file = fopen(storage->log_filename, "a");
        fprintf(log_file, "replacefile:%s,%dbytes [%s]\n", victim->metadata.filename, victim->metadata.size, get_timestamp());
        fclose(log_file);

        free_bytes = storage->size.size_bytes - storage->size.occupied_bytes + victim->metadata.size;
        result_size++;
    }

    result_size++;

    victims = malloc(result_size * sizeof(f_el));
    memset(victims, 0, sizeof(f_el));

    for(i = 0; i < result_size - 1; i++) {
        victim = select_victim(ht, storage->size.size_ht, exonerated);

        strcpy(victims[i].metadata.filename, victim->metadata.filename);

        if(victim->data != NULL) {
            victims[i].data = malloc((strlen(victim->data) + 1) * sizeof(char));

            strcpy(victims[i].data, victim->data);
        } else {
            victims[i].data = malloc(2 * sizeof(char));

            strcpy(victims[i].data, " ");
        }

        if(delete_file(storage, victim) == -1) {
            return NULL;
        }

        storage->statistics.replaced_files += 1;
    }

    victims[result_size - 1].data = NULL;

    return victims;
}

f_el *replace_file(storage *storage) {
    FILE *log_file;

    f_el **ht;
    f_el *victim;

    f_el *result;

    if(storage == NULL) {
        errno = EINVAL;

        return NULL;
    }

    if(storage->ht == NULL) {
        errno = EINVAL;

        return NULL;
    }

    ht = storage->ht;

    victim = select_victim(ht, storage->size.size_ht, NULL);
    
    if(victim == NULL) {
        errno = EPERM;

        return NULL;
    }

    printf("WORKER: il file %s, di dimensione %dbytes, verrà rimpiazzato\n", victim->metadata.filename, victim->metadata.size);

    log_file = fopen(storage->log_filename, "a");
    fprintf(log_file, "replacefile:%s,%dbytes [%s]\n", victim->metadata.filename, victim->metadata.size, get_timestamp());
    fclose(log_file);

    result = malloc(sizeof(f_el));

    strcpy(result->metadata.filename, victim->metadata.filename);

    if(victim->data != NULL) {
        result->data = malloc((strlen(victim->data) + 1) * sizeof(char));

        strcpy(result->data, victim->data);
    } else {
        result->data = malloc(2 * sizeof(char));

        strcpy(result->data, " ");
    }

    if(delete_file(storage, victim) == -1) {

        return NULL;
    }

    storage->statistics.replaced_files += 1;

    return result;
}

int read_n_files_size(f_el **ht, int size, int n, int socket_fd) {
    f_el *iterator;

    int index;
    int result = 0;
    int remaining = n;

    if(ht == NULL || size <= 0 || n < 0 || socket_fd < 0) {
        errno = EINVAL;

        return -1;
    }

    if(n == 0) {
        remaining = 1;
    }

    for(index = 0; index < size; index++) {
        iterator = ht[index];

        while(iterator != NULL && remaining > 0) {
            // Verifica se il file è in stato locked
            if(check_locked(iterator, socket_fd) != -1) {
                if(iterator->data != NULL) {
                    result += strlen(iterator->metadata.filename) + 1 + strlen(iterator->data) + 1;
                } else {
                    result += strlen(iterator->metadata.filename) + 3;
                }

                if(n != 0) {
                    remaining--;
                }
            }

            iterator = iterator->metadata.next_file;
        }
    }

    return result;
}

int set_read_n_files(f_el **ht, int size, int n, char *result, FILE *log_file, int socket_fd) {
    f_el *iterator;

    struct timespec time;

    int index;
    int remaining = n;
    int first = 1;

    char delimiter[2] = {1, '\0'};

    if(ht == NULL || size <= 0 || n < 0 || log_file == NULL || socket_fd < 0) {
        errno = EINVAL;

        return -1;
    }

    if(n == 0) {
        remaining = 1;
    }

    for(index = 0; index < size; index++) {
        iterator = ht[index];

        while(iterator != NULL && remaining > 0) {
            if(check_locked(iterator, socket_fd) != -1) {
                if(first) {
                    strcpy(result, iterator->metadata.filename);

                    first = 0;
                } else {
                    strcat(result, delimiter);
                    strcat(result, iterator->metadata.filename);
                }

                if(iterator->data != NULL) {
                    strcat(result, delimiter);
                    strcat(result, iterator->data);
                } else {
                    strcat(result, delimiter);
                    strcat(result, " ");
                }

                clock_gettime(CLOCK_REALTIME, &time);
                iterator->metadata.last_used = (long long int)time.tv_sec * 1000000000L + (long long int)time.tv_nsec;

                fprintf(log_file, "readinfo:%s,%d [%s]\n", iterator->metadata.filename, iterator->metadata.size, get_timestamp());
                fprintf(log_file, "read:%d\n", iterator->metadata.size);

                if(n != 0) {
                    remaining--;
                }
            }

            iterator = iterator->metadata.next_file;
        }
    }

    return 0;
}

int clean_closed_conn(storage *storage, int socket_fd, int max) {
    f_el **ht;
    int size;

    if(storage == NULL || storage->ht == NULL || socket_fd < 0 || max <= 0) {
        errno = EINVAL;
        
        return -1;
    }

    ht = storage->ht;
    size = storage->size.size_ht;

    if((errno = pthread_mutex_lock(&lock_storage)) != 0) {
        return -1;
    }
    
    clean_ht(ht, size, socket_fd, max);

    pthread_mutex_unlock(&lock_storage);

    return 0;
}

void write_no_content(storage *storage) {
    FILE *log_file;

    if((errno = pthread_mutex_lock(&lock_storage)) != 0) {
        return;
    }

    log_file = fopen(storage->log_filename, "a");
    fprintf(log_file, "write:0\n");
    fclose(log_file);

    pthread_mutex_unlock(&lock_storage);
}

char *get_timestamp() {
    time_t act_time = time(NULL);
    char *result = ctime(&act_time);

    result[strlen(result) - 1] = '\0';

    return result;
}
/*
 * Funzioni dell'api 
 */
f_el *openFile(storage *storage, char *filename, int flags, int socket_fd, int max) {
    f_el **ht;

    f_el *file;

    f_el *victim = NULL;

    int created = 0;

    // Verifica se filename e storage sono validi
    if(storage == NULL || storage->ht == NULL || filename == NULL || socket_fd < 0 || max <= 0) {
        errno = EINVAL;

        return NULL;
    }

    ht = storage->ht;

    if(strlen(filename) > UNIX_PATH_MAX) {
        errno = ENAMETOOLONG;

        return NULL;
    }

    // Filename è valido

    if((errno = pthread_mutex_lock(&lock_storage)) != 0) {

        return NULL;
    }

    // Verifica se esiste già un file t.c file->filename == filename
    if((file = lookup(ht, storage->size.size_ht, filename)) == NULL) {
        // Il file non esiste verifica se il flag O_CREATE è impostato
        if((flags & O_CREATE) == 0) {
            // Il flag non è impostato, l'operazione fallisce
            errno = ENOENT;

            pthread_mutex_unlock(&lock_storage);

            return NULL;
        }

        errno = 0;
        // Il file non esiste ma il flag O_CREATE è impostato, quindi crea un nuovo file
        if((victim = create_file(storage, filename, max)) == NULL && errno != 0) {

            pthread_mutex_unlock(&lock_storage);

            return NULL;
        }

        created = 1;
    }

    // Il file esiste oppure è stato creato, verifica se il file è stato creato o era già esistente 

    if(created == 0) {
        // Il file era già esistente, verifica se il flag O_CREATE è impostato 
        if((flags & O_CREATE) != 0) {
            // Il flag è impostato, l'operazione fallisce
            errno = EEXIST;

            pthread_mutex_unlock(&lock_storage);

            return NULL;
        }
    } else {
        file = lookup(ht, storage->size.size_ht, filename);
    }

    // Il file è stato creato, oppure era già esistente e il flag O_CREATE non è impostato   

    // Verifica se il file è stato già aperto dall'utente che ha richiesto l'operazione
    if(check_opened(file, socket_fd, max) == 1) {
        errno = EBADR;

        pthread_mutex_unlock(&lock_storage);

        return NULL;
    }

    // Il file non è stato ancora aperto

    // Verifica se il flag O_LOCK è impostato
    if(flags & O_LOCK) {
        // Il flag è impostato, esegue il lock del file
        if(lock_file(storage, file, socket_fd, 1) == -1) {
            errno = EPERM;

            pthread_mutex_unlock(&lock_storage);

            return NULL;
        }

        open_file(storage, file, socket_fd, max, 0);
    } else {
        open_file(storage, file, socket_fd, max, 1);
    }

    pthread_mutex_unlock(&lock_storage);

    return victim;
}

int closeFile(storage *storage, char *filename, int socket_fd, int max) {
    f_el **ht;
    f_el *file;

    if(storage == NULL || storage->ht == NULL || filename == NULL || socket_fd < 0 || max < 0) {
        errno = EINVAL;

        return -1;
    }

    ht = storage->ht;

    if(strlen(filename) > UNIX_PATH_MAX) {
        errno = ENAMETOOLONG;

        return -1;
    }

    // Filename è valido

    if((errno = pthread_mutex_lock(&lock_storage)) != 0) {
        return -1;
    }

    // Verifica se esiste un file t.c file->filename == filename
    if((file = lookup(ht, storage->size.size_ht, filename)) == NULL) {
        // Il file non esiste, l'operazione fallisce
        errno = ENOENT;

        pthread_mutex_unlock(&lock_storage);

        return -1;
    }

    if(close_file(storage, file, socket_fd, max) == -1) {

        return -1;
    } 

    pthread_mutex_unlock(&lock_storage);

    return 0;
}

f_el *writeFile(storage *storage, char *filename, int socket_fd, char *content, int max) {
    FILE *log_file;

    f_el **ht;
    f_el *victims = NULL;
    f_el *file;

    struct timespec time;

    char *file_content;

    // Verifica se i parametri sono validi
    if(storage == NULL || storage->ht == NULL || filename == NULL || socket_fd < 0 || content == NULL || max <= 0) {
        errno = EINVAL;

        return NULL;
    }

    ht = storage->ht;

    if(strlen(filename) > UNIX_PATH_MAX) {
        errno = ENAMETOOLONG;

        return NULL;
    }

    if((errno = pthread_mutex_lock(&lock_storage)) != 0) {
        return NULL;
    }

    // Verifica se il file con file->filename == filename esiste
    if((file = lookup(ht, storage->size.size_ht, filename)) == NULL) {
        errno = ENOENT;

        pthread_mutex_unlock(&lock_storage);

        return NULL;
    }

    // Verifica se il file è in stato locked
    if(check_locked(file, socket_fd) == -1) {
        errno = EPERM;

        pthread_mutex_unlock(&lock_storage);

        return NULL;
    }

    // Verifica se il file è stato aperto dall'utente che ha richiesto l'operazione
    if(check_opened(file, socket_fd, max) == 0) {
        // Il file non è stato aperto 
        errno = EBADF;

        pthread_mutex_unlock(&lock_storage);

        return NULL;
    }

    // Verifica se c'è sufficiente spazio nello storage
    if(strlen(content) > storage->size.size_bytes) {
        errno = ENOMEM;

        pthread_mutex_unlock(&lock_storage);

        return NULL;
    }

    if(strlen(content) > (storage->size.size_bytes - storage->size.occupied_bytes)) {
        printf("WORKER: È necessario il rimpiazzamento di uno o più file, spazio richiesto: %ld\n", strlen(content));
        victims = replace_files(storage, strlen(content), file->metadata.filename);
    }

    // Alloca la stringa per contenere il contenuto del file
    file_content = malloc((strlen(content) + 1) * sizeof(char));

    strcpy(file_content, content);

    // Verifica se il file aveva già un contenuto
    if(file->data != NULL) {
        storage->size.occupied_bytes = storage->size.occupied_bytes - strlen(file->data);

        free(file->data);
    }

    file->data = file_content;
    clock_gettime(CLOCK_REALTIME, &time);
    file->metadata.last_used = (long long int)time.tv_sec * 1000000000L + (long long int)time.tv_nsec;
    file->metadata.last_used = (long long int)time.tv_sec * 1000000000L + (long long int)time.tv_nsec;
    file->metadata.size = strlen(file->data);

    log_file = fopen(storage->log_filename, "a");
    fprintf(log_file, "writeinfo:%s,%d [%s]\n", file->metadata.filename, file->metadata.size, get_timestamp());
    fprintf(log_file, "write:%d\n", file->metadata.size);
    fclose(log_file);

    storage->size.occupied_bytes += strlen(file->data);
    if(storage->size.occupied_bytes > storage->statistics.max_stored_bytes) {
        storage->statistics.max_stored_bytes = storage->size.occupied_bytes;
    }

    pthread_mutex_unlock(&lock_storage);

    return victims;
}

char *readFile(storage *storage, char *filename, int socket_fd) {
    FILE *log_file;

    struct timespec time;

    f_el *file;

    char *result;

    if(storage == NULL || storage->ht == NULL || filename == NULL || socket_fd < 0) {
        errno = EINVAL;

        return 0;
    }

    if(strlen(filename) > UNIX_PATH_MAX) {
        errno = ENAMETOOLONG;

        return NULL;
    }

    if((errno = pthread_mutex_lock(&lock_storage)) != 0) {
        return NULL;
    }

    // Verifica se il file con file->filename == filename esiste
    if((file = lookup(storage->ht, storage->size.size_ht, filename)) == NULL) {
        errno = ENOENT;

        pthread_mutex_unlock(&lock_storage);

        return NULL;
    }

    // Verifica se il file è in stato locked
    if(check_locked(file, socket_fd) == -1) {
        errno = EPERM;

        pthread_mutex_unlock(&lock_storage);

        return NULL;
    }

    log_file = fopen(storage->log_filename, "a");
    fprintf(log_file, "readinfo:%s,%d [%s]\n", file->metadata.filename, file->metadata.size, get_timestamp());
    fprintf(log_file, "read:%d\n", file->metadata.size);
    fclose(log_file);

    pthread_mutex_unlock(&lock_storage);

    clock_gettime(CLOCK_REALTIME, &time);
    file->metadata.last_used = (long long int)time.tv_sec * 1000000000L + (long long int)time.tv_nsec;

    // Il file esiste
    if(file->data != NULL) {
        result = file->data;
    } else {
        result = " ";
    }

    return result;
}

char *readNFiles(storage *storage, int n, int socket_fd) {
    FILE *log_file;

    char *result;
    int result_size = 0;

    if(storage == NULL || storage->ht == NULL || n < 0 || socket_fd < 0) {
        errno = EINVAL;

        return NULL;
    }

    if((errno = pthread_mutex_lock(&lock_storage)) != 0) {
        return NULL;
    }

    result_size = read_n_files_size(storage->ht, storage->size.size_ht, n, socket_fd);

    result = malloc((result_size + 1) * sizeof(char));

    strcpy(result, "");

    log_file = fopen(storage->log_filename, "a");
    set_read_n_files(storage->ht, storage->size.size_ht, n, result, log_file, socket_fd);
    fclose(log_file);

    pthread_mutex_unlock(&lock_storage);

    return result;
}

f_el *appendToFile(storage *storage, char *filename, int socket_fd, char *content, int max) {
    FILE *log_file;
    f_el **ht;

    f_el *victims = NULL;
    f_el *file;
    char *file_content;

    struct timespec time;

    // Verifica se i parametri sono validi
    if(storage == NULL || storage->ht == NULL || filename == NULL || socket_fd < 0 || content == NULL || max <= 0) {
        errno = EINVAL;

        return NULL;
    }

    if(strlen(filename) > UNIX_PATH_MAX) {
        errno = ENAMETOOLONG;

        return NULL;
    }

    ht = storage->ht;

    if((errno = pthread_mutex_lock(&lock_storage)) != 0) {
        return NULL;
    }

    // Verifica se il file con file->filename == filename esiste
    if((file = lookup(ht, storage->size.size_ht, filename)) == NULL) {
        errno = ENOENT;

        return NULL;
    }

    // Verifica se il file è in stato locked
    if(check_locked(file, socket_fd)) {
        errno = EPERM;

        pthread_mutex_unlock(&lock_storage);

        return NULL;
    }

    // Verifica se il file è stato aperto dall'utente che ha richiesto l'operazione
    if(check_opened(file, socket_fd, max)) {
        // Il file non è stato aperto 
        errno = EBADF;

        pthread_mutex_unlock(&lock_storage);

        return NULL;
    }

    // Verifica se c'è sufficiente spazio nello storage
    if(file->data != NULL) {
        if(strlen(file->data) + strlen(content) > storage->size.size_bytes){
            errno = ENOMEM;

            return NULL;
        }
    }

    if(strlen(content) >= (storage->size.size_bytes - storage->size.occupied_bytes)) {
        printf("È necessario un rimpiazzamento del file, spazio richiesto: %ld\n", strlen(content));
        victims = replace_files(storage, strlen(content), file->metadata.filename);
    }

    // Alloca la stringa per contenere il contenuto del file

    if(file->data != NULL) {
        file_content = malloc((strlen(file->data) + strlen(content) + 1) * sizeof(char));
    } else {
        file_content = malloc((strlen(content) + 1) * sizeof(char));
    }

    strcpy(file_content, file->data);
    strcat(file_content, content);

    if(file->data != NULL) {
        free(file->data);
    }

    file->data = file_content;
    clock_gettime(CLOCK_REALTIME, &time);
    file->metadata.last_used = (long long int)time.tv_sec * 1000000000L + (long long int)time.tv_nsec;
    file->metadata.size = strlen(file->data);

    log_file = fopen(storage->log_filename, "a");
    fprintf(log_file, "writeinfo:%s,%d [%s]\n", file->metadata.filename, file->metadata.size, get_timestamp());
    fprintf(log_file, "write:%d\n", file->metadata.size);
    fclose(log_file);

    storage->size.occupied_bytes = storage->size.occupied_bytes + strlen(file->data);
    if(storage->size.occupied_bytes > storage->statistics.max_stored_bytes) {
        storage->statistics.max_stored_bytes = storage->size.occupied_bytes;
    }

    pthread_mutex_unlock(&lock_storage);

    return victims;
}

int lockFile(storage *storage, char *filename, int socket_fd) {
    f_el **ht;
    f_el *file;

    struct timespec time;

    int check;

    if(storage == NULL || storage->ht == NULL || filename == NULL || socket_fd < 0) {
        errno = EINVAL;

        return -1;
    }

    ht = storage->ht;
 
    if((errno = pthread_mutex_lock(&lock_storage)) != 0) {
        return -1;
    }
    
    file = lookup(ht, storage->size.size_ht, filename);

    // Verifica se il file esiste
    if(file == NULL) {
        // Il file non esiste
        errno = ENOENT;

        pthread_mutex_unlock(&lock_storage);

        return -1;
    }
    
    // Il file esiste

    if((check = check_locked(file, socket_fd)) == -1) {
        errno = EPERM;

        pthread_mutex_unlock(&lock_storage);

        return -1;
    }

    if(check == 0) {
        if(lock_file(storage, file, socket_fd, 0) == -1) {
            pthread_mutex_unlock(&lock_storage);

            return -1;
        }
    }

    clock_gettime(CLOCK_REALTIME, &time);
    file->metadata.last_used = (long long int)time.tv_sec * 1000000000L + (long long int)time.tv_nsec;

    pthread_mutex_unlock(&lock_storage);

    return 0;
}

int unlockFile(storage *storage, char *filename, int socket_fd) {
    f_el **ht;
    f_el *file;

    struct timespec time;

    if(storage == NULL || storage->ht == NULL || filename == NULL || socket_fd < 0) {
        errno = EINVAL;

        return -1;
    }

    ht = storage->ht;
 
    if((errno = pthread_mutex_lock(&lock_storage)) != 0) {
        return -1;
    }
    
    file = lookup(ht, storage->size.size_ht, filename);

    // Verifica se il file esiste
    if(file == NULL) {
        // Il file non esiste
        errno = ENOENT;

        pthread_mutex_unlock(&lock_storage);

        return -1;
    }
    
    // Il file esiste

    if(check_locked(file, socket_fd) != 1) {
        errno = EPERM;

        pthread_mutex_unlock(&lock_storage);

        return -1;
    }

    if(unlock_file(storage, file, socket_fd) == -1) {
        pthread_mutex_unlock(&lock_storage);

        return -1;
    }

    clock_gettime(CLOCK_REALTIME, &time);
    file->metadata.last_used = (long long int)time.tv_sec * 1000000000L + (long long int)time.tv_nsec;

    pthread_mutex_unlock(&lock_storage);

    return 0;
}

int removeFile(storage *storage, char *filename, int socket_fd) {
    FILE *log_file;

    f_el **ht;
    f_el *file;
    
    if(storage == NULL || storage->ht == NULL || filename == NULL || socket_fd < 0) {
        errno = EINVAL;

        return -1;
    }

    ht = storage->ht;

    if((errno = pthread_mutex_lock(&lock_storage)) != 0) {
        return -1;
    }

    file = lookup(ht, storage->size.size_ht, filename);

    // Verifica se il file esiste
    if(file == NULL) {
        // Il file non esiste
        errno = ENOENT;
        
        pthread_mutex_unlock(&lock_storage);

        return -1;
    }

    // Verifica se il file è già locked
    if(check_locked(file, socket_fd) != 1) {
        errno = EPERM;

        pthread_mutex_unlock(&lock_storage);

        return -1;
    }

    if(delete_file(storage, file) == -1) {
        pthread_mutex_unlock(&lock_storage);

        return -1;
    }

    log_file = fopen(storage->log_filename, "a");
    fprintf(log_file, "removefile:%s [%s]\n", filename, get_timestamp());
    fclose(log_file);

    pthread_mutex_unlock(&lock_storage);

    return 0;
}