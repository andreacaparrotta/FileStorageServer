#define UNIX_PATH_MAX 108

struct worker_arg{
    request_queue_el **head_request;
    resolved_queue_el **head_resolved;
    resolved_queue_el **tail_resolved;
    int thread_n;
};

typedef struct worker_arg worker_arg;

void *main_worker(void *arg);

int check_request(char *request, int socket_fd);

void *main_worker(void *arg) {
    worker_arg *args = (worker_arg *)arg;

    request_queue_el **head_request = args->head_request;
    resolved_queue_el **head_resolved = args->head_resolved;
    resolved_queue_el **tail_resolved = args->tail_resolved;

    resolved_queue_el *push_result;

    struct sigaction signal;

    void *request;
    int socket_fd;
    int thread_n = args->thread_n;

    int result;

    memset(&signal, 0, sizeof(signal));

    signal.sa_handler = SIG_IGN;

    if(sigaction(SIGPIPE, &signal, NULL) == -1) {
        printf("WORKER %d:", thread_n);
        perror("Setting new sigpipe handler");

        pthread_exit((void *)1);
    }

    request = malloc(128 * sizeof(char));

    while(1) {
        if((socket_fd = pop_request(head_request)) == -1) {
            printf("WORKER %d:", thread_n);
            perror("Obtaining request: ");

        } else {
            memset(request, 0, 128);
            //Richiesta ricevuta
            if(read(socket_fd, request, 128) == -1) {
                printf("WORKER %d:", thread_n);
                perror("Reading from socket");
            }

            fprintf(stderr, "WORKER %d: %s\n", thread_n,(char *)request);

            if((result = check_request((char *)request, socket_fd)) == -1) {
                printf("WORKER %d:", thread_n);
                perror("Checking request");

                if(write(socket_fd, "1", 1) == -1) {
                    //La connessione deve essere chiusa
                    if(push_resolved(head_resolved, tail_resolved, socket_fd, 1) == NULL) {
                        printf("WORKER %d:", thread_n);
                        perror("Pushing resolved request");
                    }
                }
            } else if(result == 0) {
                //La connessione rimane aperta per altre richieste
                if(push_resolved(head_resolved, tail_resolved, socket_fd, 0) == NULL) {
                    printf("WORKER %d:", thread_n);
                    perror("Pushing resolved request");
                }
            } else if(result == 1) {
                //La connessione deve essere chiusa
                if(push_resolved(head_resolved, tail_resolved, socket_fd, 1) == NULL) {
                    printf("WORKER %d:", thread_n);
                    perror("Pushing resolved request");
                }
            }
        }
    }

    free(request);
}

int check_request(char *request, int socket_fd) {
    char *request_code = strtok(request, ",");

    if(request_code != NULL && strcmp(request_code, "OPENFILE") == 0) {
        char *pathname = strtok(NULL, ",");
        char *flags = strtok(NULL, ",");

        if(write(socket_fd, "0", 1) == -1) {
            perror("WORKER: Writing to client");
        }

        return 0;
    } else if(request_code != NULL && strcmp(request_code, "CLOSECONN") == 0){
        close(socket_fd);

        return 1;
    } else {
        errno = EINVAL;

        return -1;
    }
}
