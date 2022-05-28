#include <pthread.h>
#include <errno.h>

struct resolved_queue_el {
    int resolved_fd;
    int close;
    struct resolved_queue_el *next_resolved;
};

typedef struct resolved_queue_el resolved_queue_el;

pthread_mutex_t lock_resolved_queue = PTHREAD_MUTEX_INITIALIZER;

/*
 * Inserisce un nuovo elemento nella queue
 * Parametri:
 *      head: il puntatore al puntatore alla testa della queue
 *      tail: il puntatore al puntatore alla coda della queue
 *      fd: il file descriptor da inserire nella queue
 *      close: indica se il file descriptor deve essere chiuso, 1 se deve essere chiuso, 0 altrimenti
 * Errno:
 *      EINVAL: se head == NULL, tail == NULL oppure fd < 0
 * Ritorna: il nuovo elemento in caso di successo, NULL altrimenti
 */
resolved_queue_el *push_resolved(resolved_queue_el **head, resolved_queue_el **tail, int fd, int close);

/*
 * Rimuove un elemento dalla queue
 * Parametri:
 *      head: il puntatore al puntatore alla testa della queue
 * Ritorna: una struct contenente il file descriptor e l'indicazione sulla necessità di chiudere il file descriptor, se la queue è vuota allora il file descriptor è impostato a -1
 */
resolved_queue_el pop_resolved(resolved_queue_el **head);

/*
 * Visualizza sullo standard output il contenuto della queue
 * Parametri:
 *      head: il puntatore alla testa della queue
 * Ritorna: none
 */
void print_resolved_queue(resolved_queue_el *head);

/*
 * Dealloca gli elementi contenuti nella queue
 * Parametri:
 *      head: il puntatore alla testa della queue
 * Ritorna: none
 */
void free_resolved_queue(resolved_queue_el *head);

resolved_queue_el *push_resolved(resolved_queue_el **head, resolved_queue_el **tail, int fd, int close) {
    resolved_queue_el *n_el;

    if(*head != NULL && tail == NULL) {
        errno = EINVAL;

        return NULL;
    }

    if(fd < 0) {
        errno = EINVAL;

        return NULL;
    }

    // Alloca e inizializza il nuovo elemento
    if((n_el = malloc(sizeof(resolved_queue_el))) == NULL) {
        return NULL;
    }
    n_el->resolved_fd = fd;
    n_el->close = close;
    n_el->next_resolved = NULL;

    if((errno = pthread_mutex_lock(&lock_resolved_queue)) != 0) {
        return NULL;
    }

    if(*head == NULL) {
        // La queue è vuota, modifica la testa
        *head = n_el;
        *tail = n_el;
    } else {
        // La queue non è vuota, modifica la coda
        (*tail)->next_resolved = n_el;
        *tail = n_el;
    }

    pthread_mutex_unlock(&lock_resolved_queue);

    return n_el;
}

resolved_queue_el pop_resolved(resolved_queue_el **head) {
    resolved_queue_el result;
    resolved_queue_el *old_head;

    if((errno = pthread_mutex_lock(&lock_resolved_queue)) != 0) {

        result.resolved_fd = -1;
        return result;
    }

    if(*head != NULL) {
        // È presente un elemento nella queue
        old_head = *head;

        *head = old_head->next_resolved;

        result.resolved_fd = old_head->resolved_fd;
        result.close = old_head->close;

        free(old_head);
    } else {
        // La queue è vuota
        result.resolved_fd = -1;
    }

    pthread_mutex_unlock(&lock_resolved_queue);

    return result;
}

void print_resolved_queue(resolved_queue_el *head){
    if(head == NULL) {
        fprintf(stderr, "NULL\n");
    }

    if(head != NULL) {
        fprintf(stderr, "%d->", head->resolved_fd);

        print_resolved_queue(head->next_resolved);
    }
}

void free_resolved_queue(resolved_queue_el *head) {
    if(head == NULL) {
        return;
    }

    free_resolved_queue(head->next_resolved);

    free(head);
}