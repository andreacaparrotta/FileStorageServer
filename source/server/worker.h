#define UNIX_PATH_MAX 108

void *main_worker(void *arg);

void check_request(char *request, int socket_fd);

void *main_worker(void *arg) {
    request_queue_el **head = (request_queue_el **)arg;
    void *request;
    int socket_fd;

    request = malloc(128 * sizeof(char));

    while(1) {
        if((socket_fd = pop_request(head)) == -1) {
            perror("Obtaining request: ");

        } else {
            //Richiesta ricevuta
            if(read(socket_fd, request, 128) == -1) {
                perror("Reading from socket");
            }

            check_request((char *)request, socket_fd);
        }
    }
}

void check_request(char *request, int socket_fd) {
     char *request_code = strtok(request, ",");

    if(strcmp(request_code, "OPENFILE") == 0) {
        char *pathname = strtok(NULL, ",");
        char *flags = strtok(NULL, ",");


        //fprintf(stderr, "il percorso è %s con flag %s", pathname, flags);

        write(socket_fd, "0", 1);
    }
}
