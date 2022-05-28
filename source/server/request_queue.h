#include <pthread.h>
#include <errno.h>

struct request_queue_el {
    int request_fd;
    struct request_queue_el *next_request;
};

typedef struct request_queue_el request_queue_el;

pthread_mutex_t lock_queue = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_queue = PTHREAD_COND_INITIALIZER;

/*
 * Inserisce un nuovo elemento nella queue
 * Parametri:
 *      head: il puntatore al puntatore alla testa della queue
 *      tail: il puntatore al puntatore alla coda della queue
 *      fd: il file descriptor da inserire nella queue
 * Errno:
 *      EINVAL: se head == NULL, tail == NULL oppure fd < 0
 * Ritorna: il nuovo elemento in caso di successo, NULL altrimenti
 */
request_queue_el *push_request(request_queue_el **head, request_queue_el **tail, int fd);

/*
 * Rimuove un elemento dalla queue, se la queue è vuota il thread che invoca questa funzione si mette in attesa
 * Parametri:
 *      head: il puntatore al puntatore alla testa della queue
 * Ritorna: il file descriptor presente in testa alla queue
 */
int pop_request(request_queue_el **head);

/*
 * Visualizza sullo standard output il contenuto della queue
 * Parametri:
 *      head: il puntatore alla testa della queue
 * Ritorna: none
 */
void print_queue(request_queue_el *head);

/*
 * Verifica l'esistenza del file descriptor fd nella queue
 * Parametri:
 *      head: il puntatore alla testa della queue
 *      fd: il file descriptor su cui si vuole eseguire la verifica
 * Ritorna: 1 se il file descriptor è stato trovato, 0 altrimenti
 */
int exist_fd(request_queue_el *head, int fd);

/*
 * Dealloca gli elementi contenuti nella queue
 * Parametri:
 *      head: il puntatore alla testa della queue
 * Ritorna: none
 */
void free_request_queue(request_queue_el *head);

request_queue_el *push_request(request_queue_el **head, request_queue_el **tail, int fd) {
    request_queue_el *n_el;

    if(*head != NULL && *tail == NULL) {
        errno = EINVAL;

        return NULL;
    }

    if(fd < 0) {
        errno = EINVAL;

        return NULL;
    }

    // Crea e inizializza il nuovo elemento della queue
    if((n_el = malloc(sizeof(request_queue_el))) == NULL) {
        return NULL;
    }
    n_el->request_fd = fd;
    n_el->next_request = NULL;

    if((errno = pthread_mutex_lock(&lock_queue)) != 0) {
        return NULL;
    }

    if(*head == NULL) {
        // La queue è vuota, bisogna modificare la testa
        *head = n_el;
        *tail = n_el;
    } else {
        // La queue non è vuota, bisogna modificare la coda
        (*tail)->next_request = n_el;
        *tail = n_el;
    }

    // Informa i thread consumatori che un nuovo elemento è disponibile
    if((errno = pthread_cond_signal(&cond_queue)) != 0) {
        pthread_mutex_unlock(&lock_queue);

        return NULL;
    }

    pthread_mutex_unlock(&lock_queue);

    return n_el;
}

int pop_request(request_queue_el **head) {
    int result;
    request_queue_el *old_head;

    if((errno = pthread_mutex_lock(&lock_queue)) != 0) {

        return -1;
    }

    // Verifica se è presente un elemento nella queue, in caso contrario si mette in attesa fino a che un nuovo elemento è disponibile
    while(*head == NULL) {
        if((errno = pthread_cond_wait(&cond_queue, &lock_queue)) != 0) {
            pthread_mutex_unlock(&lock_queue);

            return -1;
        }
    }

    // Rimuove l'elemento dalla queue
    old_head = *head;
    *head = old_head->next_request;
    result = old_head->request_fd;

    pthread_mutex_unlock(&lock_queue);

    free(old_head);

    return result;
}

void print_queue(request_queue_el *head){
    if(head == NULL) {
        fprintf(stderr, "NULL\n");
    }

    if(head != NULL) {
        fprintf(stderr, "%d->", head->request_fd);

        print_queue(head->next_request);
    }
}

int exist_fd(request_queue_el *head, int fd) {
    if(head == NULL) {
        // Il file descriptor non è stato trovato
        return 0;
    }

    if(head->request_fd == fd) {
        // Il file descriptor è stato trovato
        return 1;
    }

    return exist_fd(head->next_request, fd);
}

void free_request_queue(request_queue_el *head) {
    if(head == NULL) {
        return;
    }

    free_request_queue(head->next_request);

    free(head);
}