#define UNIX_PATH_MAX 108

struct metadata {
    char filename[UNIX_PATH_MAX];           // Filename usato come identificatore per il file
    int size;                               // Dimensione in byte del file
    long long int last_used;                // Ultimo utilizzo del file specificato in nanosecondi a partire da epoch
    int acquired_by;                        // File descriptor del socket che possiede la lock sul file
    int lock_type;                          // 1 se il file è locked a seguito di una open con lock flag, 0 se il file è locked per richiesta del client se acquired_by è uguale a -1 questo valore non deve essere considerato 
    int *opened;                            // Array che contiene i file descriptor dei socket che hanno aperto il file
    int index;                              // Indice della hash table in cui è memorizzato il file
    struct f_el *next_file;                 // File successivo contenuto nella stessa cella della hash table
    struct f_el *prev_file;                 // File precedente contenuto nella stessa cella della hash table
};

struct f_el {
    struct metadata metadata;               // Contiene i metadati necessari per la gestione dei file
    char *data;                             // Contiene i dati del file
};

typedef struct metadata metadata;
typedef struct f_el f_el;

/*
 * Calcola la dimensione della lista puntata da una cella della hash table
 * Parametri:
 *      list: il puntatore alla testa della lista
 * Ritorna: la dimensione della lista
 */
int list_size(f_el *list);

/*
 * Inserisce un nuovo file nella hash table
 * Parametri:
 *      ht: l'array che rappresenta l'hash table
 *      size: la dimensione dell'array che rappresenta la hash table
 *      n_file: il puntatore al nuovo file da inserire nella hash table
 * Errno:
 *      EINVAL: se ht oppure n_file sono uguali a NULL
 *      EEXIST: se esiste già un file t.c file->metadata.filename == n_file->metadata.filename
 * Ritorna: 0 in caso di successo, -1 in caso di errore
 */
int insert(f_el **ht, size_t size, f_el *n_file);

/*
 * Cerca un file nella lista t.c. list->metadata.filename == filename
 * Parametri:
 *      list: il puntatore alla testa della lista
 *      filename: il filename del file da cercare
 * Ritorna: il puntatore al file di interesse, NULL se il file non è presente nella lista
 */
f_el *list_lookup(f_el *list, char *filename);

/*
 * Cerca un file nella hash table per filename
 * Parametri:
 *      ht: l'array che rappresenta l'hash table
 *      size: la dimensione dell'array che rappresenta la hash table
 *      filename: il filename del file da cercare
 * Errno:
 *      EINVAL: se ht oppure filename sono uguali a NULL
 *      ENOENT: se il file non è presente nella hash table
 * Ritorna: il puntatore al file di interesse, NULL se il file non è presente nella hash table
 */
f_el *lookup(f_el **ht, size_t size, char *filename);

/*
 * Rimuove il file nella hash table con filename uguale a quello specificato come parametro
 * Parametri:
 *      ht: l'array che rappresenta l'hash table
 *      size: la dimensione dell'array che rappresenta la hash table
 *      filename: il filename del file da cercare
 * Errno:
 *      EINVAL: se ht oppure filename sono uguali a NULL
 *      ENOENT: se non esiste un file con filename uguale a quello specificato come parametro
 * Ritorna: 0 in caso di successo, -1 in caso di errore
 */
int delete(f_el **ht, size_t size, f_el *victim);

/*
 * Calcola il primo indice in cui inserire o ricercare il file
 * Parametri:
 *      filename: il filename su cui applicare la funzione hash
 *      size: la dimensione dell'array che rappresenta la hash table
 * Ritorna: l'indice in cui inserire o cercare il file
 */
int hash1(const char *filename, size_t size);

/*
 * Calcola il secondo indice in cui inserire o ricercare il file
 * Parametri:
 *      filename: il filename su cui applicare la funzione hash
 *      size: la dimensione dell'array che rappresenta la hash table
 * Ritorna: l'indice in cui inserire o cercare il file
 */
int hash2(const char* filename, size_t size);

/*
 * Seleziona un file vittima da rimuovere nel caso di raggiungimento della dimensione massima dello storage, usa una politica LRU
 * Parametri:
 *      ht: l'array che rappresenta l'hash table
 *      size: la dimensione dell'array che rappresenta la hash table
 *      exonerated: il filename di un file esonerato dalla possibile espulsione, necessario nel caso di write in cui lo stesso file in cui scrivere potrebbe essere espulso
 * Ritorna: il puntatore al file scelto come vittima, NULL se lo storage è vuoto
 */
f_el *select_victim(f_el **ht, size_t size, char *exonerated);

/*
 * Dealloca la memoria degli elementi presenti nella lista
 * Parametri:
 *      list: la lista di cui deallocare gli elementi
 * Ritorna: none
 */
void free_list(f_el* list);

/*
 * Dealloca la memoria degli elementi presenti nella hash table
 * Parametri:
 *      list: la lista di cui deallocare gli elementi
 * Errno:
 *      EINVAL: se ht è NULL oppure se size < 0
 *      
 * Ritorna: 0 in caso di successo, -1 altrimenti
 */
int free_ht(f_el** ht, size_t size);

/*
 * Visualizza sullo standard output il filename degli elementi nella lista
 * Parametri:
 *      list: la lista di cui visualizzare gli elementi
 */
void print_list(f_el *list);

/*
 * Visualizza sullo standard output il filename degli elementi nella hash table
 * Parametri:
 *      ht: l'array che rappresenta l'hash table
 *      size: la dimensione dell'array che rappresenta la hash table
 */
void print_ht(f_el **ht, size_t size);

/* Ripristina lock e file aperti dal client identificato da socket_fd, per i file contenuti nella lista
 * Parametri:
 *      list: la lista di file da ripristinare
 *      socket_fd: il descrittore su cui basare il ripristino
 *      max: il numero massimo di connessioni contemporaneamente attive
 */
void clean_list(f_el *list, int socket_fd, int max);

/* Ripristina lock e file aperti dal client identificato da socket_fd , per i file contenuti nella hash table
 * Parametri:
 *      ht: la hash table che contiene i file da ripristinare
 *      size: la dimensione dell'array che modella la hash table
 *      socket_fd: il descrittore su cui basare il ripristino
 *      max: il numero massimo di connessioni contemporaneamente attive
 */
void clean_ht(f_el **ht, int size, int socket_fd, int max);

int list_size(f_el *list) {
    if(list == NULL) {
        return 0;
    }

    return list_size(list->metadata.next_file) + 1;
}

int insert(f_el **ht, size_t size, f_el *n_file) {
    f_el *iterator;

    int index;
    int index1;
    int index2;

    if(ht == NULL || n_file == NULL) {
        errno = EINVAL;

        return -1;
    }

    if(lookup(ht, size, n_file->metadata.filename) != NULL) {
        errno = EEXIST;

        return -1;
    }

    // Necessario perchè lookup imposta errno a ENOENT 
    errno = 0;

    index1 = hash1(n_file->metadata.filename, size);

    if(ht[index1] == NULL) {
        // Non è avvenuta una collisione 
        ht[index1] = n_file;

        n_file->metadata.index = index1;

    } else {
        // È avvenuta una collisione con il primo index prova con il secondo
        index2 = hash2(n_file->metadata.filename, size);

        if(ht[index2] == NULL) {
            // Non è avvenuta una collisione 
            ht[index2] = n_file;

            n_file->metadata.index = index2;
        } else {
            // Sceglie la lista di dimensione inferiore in cui inserire il file
            if(list_size(ht[index1]) > list_size(ht[index1])) {
                index = index2;
            } else {
                index = index1;
            }

            // È avvenuta una collisione si aggiunge l'elemento nella linked list
            iterator = ht[index];

            while(iterator->metadata.next_file != NULL && strcmp(iterator->metadata.filename, n_file->metadata.filename) < 0) {
                iterator = iterator->metadata.next_file;
            }

            if(iterator->metadata.next_file == NULL && strcmp(iterator->metadata.filename, n_file->metadata.filename) < 0) {
                // Il file deve essere aggiunto in coda
                iterator->metadata.next_file = n_file;

                n_file->metadata.prev_file = iterator;
                n_file->metadata.next_file = NULL;
            } else if(iterator->metadata.prev_file == NULL && strcmp(iterator->metadata.filename, n_file->metadata.filename) > 0){
                // Il file deve essere aggiunto in testa
                ht[index] = n_file;

                n_file->metadata.prev_file = NULL;
                n_file->metadata.next_file = iterator;

                iterator->metadata.prev_file = n_file;
            } else {
                // Il file deve essere aggiunto in mezzo
                n_file->metadata.prev_file = iterator->metadata.prev_file;
                n_file->metadata.prev_file->metadata.next_file = n_file;

                n_file->metadata.next_file = iterator;
                iterator->metadata.prev_file = n_file;
            }

            n_file->metadata.index = index;
        }
    }

    return 0;
}

f_el *list_lookup(f_el *list, char *filename) {
    // Verifica se la lista è vuota oppure se è stata raggiunta la fine della lista
    if(list == NULL) {
        // Il file non è presente nella lista
        return NULL;
    }

    if(strcmp(list->metadata.filename, filename) == 0) {
        // Il file è stato trovato
        return list;
    }

    return list_lookup(list->metadata.next_file, filename);
}

f_el *lookup(f_el **ht, size_t size, char *filename) {
    f_el *result;

    int index1 = -1;
    int index2 = -1;

    if(ht == NULL || filename == NULL) {
        errno = EINVAL;

        return NULL;
    }

    index1 = hash1(filename, size);

    if(index1 == -1) {
        printf("Errore nel calcolo hash 1\n");

        return NULL;
    }

    if(ht[index1] == NULL || (result = list_lookup(ht[index1], filename)) == NULL) {
        // Nel primo indice non è presente alcun file oppure il file non è presente nella lista, verifica con il secondo indice

        index2 = hash2(filename, size);

        if(index2 == -1) {
            printf("Errore nel calcolo hash 2\n");

            return NULL;
        }

        if(ht[index2] == NULL || (result = list_lookup(ht[index2], filename)) == NULL) {
            // Nel secondo indice non è presente alcun file, quindi il file non è presente nell'hash table

            errno = ENOENT;

            return NULL;
        }
    }

    // Il file è stato trovato

    return result;
}

int delete(f_el **ht, size_t size, f_el *victim) {
    if(ht == NULL || victim == NULL) {
        errno = EINVAL;

        return -1;
    }

    // Verifica se il file è presente nella hash table
    if(lookup(ht, size, victim->metadata.filename) == NULL) {
        errno = ENOENT;

        return -1;
    }

    if(victim->metadata.next_file == NULL && victim->metadata.prev_file != NULL){
        // Deve essere eliminata la coda della lista

        victim->metadata.prev_file->metadata.next_file = NULL;
    } else if(victim->metadata.prev_file == NULL) {
        // Deve essere eliminata la testa della lista

        ht[victim->metadata.index] = victim->metadata.next_file;

        if(victim->metadata.next_file != NULL) {
            victim->metadata.next_file->metadata.prev_file = NULL;
        }
    } else {
        // Deve essere eliminato un nodo intermedio della lista

        victim->metadata.prev_file->metadata.next_file = victim->metadata.next_file;
        victim->metadata.next_file->metadata.prev_file = victim->metadata.prev_file;
    }

    free(victim->metadata.opened);

    if(victim->data != NULL) {
        free(victim->data);
    }

    free(victim);

    return 0;
}

int hash1(const char *filename, size_t size) {
    const int p = 31, m = 1e9 + 7;
    int index = 0;
    long p_pow = 1;
    int i;

    for(i = 0; i < strlen(filename); i++) {
        index = (index + (filename[i] - 'a' + 1) * p_pow) % m;
        p_pow = (p_pow * p) % m;
    }

    return index % size;
}

int hash2(const char *filename, size_t size) {
    const int p = 37, m = 1e9 + 9;
    int index = 0;
    long p_pow = 1;
    int i;

    for(i = 0; i < strlen(filename); i++) {
        index = (index + (filename[i] - 'a' + 1) * p_pow) % m;
        p_pow = (p_pow * p) % m;
    }

    return index % size;
}

f_el *select_victim(f_el **ht, size_t size, char *exonerated) {
    f_el *victim = NULL;
    f_el *iterator;

    int index;

    // Verifica se è definito un file da ignorare
    if(exonerated == NULL) {
        exonerated = "";
    }

    for(index = 0; index < size; index++) {
        iterator = ht[index];

        while(iterator != NULL) {

            if(strcmp(iterator->metadata.filename, exonerated) != 0 && iterator->metadata.size != 0) {

                // Verifica se il file puntato da iterator è stato usato meno di recente rispetto all'attuale vittima
                if(victim == NULL || victim->metadata.last_used > iterator->metadata.last_used) {
                    victim = iterator;
                }
            }

            iterator = iterator->metadata.next_file;
        }
    }

    return victim;
}

void free_list(f_el* list) {
    if(list == NULL) {
        return;
    }

    free_list(list->metadata.next_file);

    free(list->metadata.opened);
    free(list->data);
    free(list);


    return;
}

int free_ht(f_el** ht, size_t size) {
    f_el *list;

    int i;

    if(ht == NULL || size < 0) {
        errno = EINVAL;

        return -1;
    }

    for(i = 0; i < size; i++) {
        list = ht[i];

        free_list(list);

        ht[i] = NULL;
    }

    return 0;
}

void print_list(f_el *list) {
    if(list == NULL) {
        printf("NULL\n");

        return;
    }

    printf("%s -> ", list->metadata.filename);

    print_list(list->metadata.next_file);
}

void print_ht(f_el **ht, size_t size) {
    int i;
    printf("\n");
    for(i = 0; i < size; i++) {
        if(ht[i] != NULL) {
            printf("%d -> ", i);

            print_list(ht[i]);
        }
    }
    printf("\n");
}

void clean_list(f_el *list, int socket_fd, int max) {
    int i;

    if(list == NULL) {
        return;
    }

    if(list->metadata.acquired_by == socket_fd) {
        list->metadata.acquired_by = -1;
    }

    for(i = 0; i < max; i++) {
        if(list->metadata.opened[i] == socket_fd) {
            list->metadata.opened[i] = -1;
        }
    }
}

void clean_ht(f_el **ht, int size, int socket_fd, int max) {
    int i;

    for(i = 0; i < size; i++) {
        clean_list(ht[i], socket_fd, max);
    }
}