#include <pthread.h>
#include <errno.h>

struct request_queue_el {
    int request_fd;
    struct request_queue_el *next_request;
};

typedef struct request_queue_el request_queue_el;

pthread_mutex_t lock_queue = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_queue = PTHREAD_COND_INITIALIZER;

//Inserisce il un nuovo socket file descriptor fd in coda dopo tail
//return: il nuovo ultimo elemento se l'operazione avviene correttamente, NULL altrimenti e imposta errno
request_queue_el *push_request(request_queue_el **head, request_queue_el *tail, int fd);

//Restituisce il puntatore al valore del file descriptor che si trova in testa alla coda se la coda è vuota blocca il thread in attesa di una richiesta
//return: il valore del file descriptor che si trova in testa alla coda, -1 altrimenti  e imposta errno
int pop_request(request_queue_el **head);

//Visualizza il contenuto della coda sullo standard output
void print_queue(request_queue_el *head);

int exist_fd(request_queue_el *head, int fd);

request_queue_el *push_request(request_queue_el **head, request_queue_el *tail, int fd) {
    request_queue_el *n_el;

    if(*head != NULL && tail == NULL) {
        errno = EINVAL;

        return NULL;
    }

    if(fd < 0) {
        errno = EINVAL;

        return NULL;
    }

    if((n_el = malloc(sizeof(request_queue_el))) == NULL) {
        return NULL;
    }

    n_el->request_fd = fd;
    n_el->next_request = NULL;

    if((errno = pthread_mutex_lock(&lock_queue)) != 0) {
        return NULL;
    }

    if(*head == NULL) {
        *head = n_el;
    } else {
        tail->next_request = n_el;
    }

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

    while(*head == NULL) {
        if((errno = pthread_cond_wait(&cond_queue, &lock_queue)) != 0) {
            pthread_mutex_unlock(&lock_queue);

            return -1;
        }
    }

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
        return 0;
    }

    if(head->request_fd == fd) {
        return 1;
    }

    return exist_fd(head->next_request, fd);
}
