#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <sys/un.h>
#include <string.h>
#include <math.h>

#include "response.h"

#define UNIX_PATH_MAX 108
#define REQUEST_BUFF_SIZE 256
#define RESPONSE_BUFF_SIZE 256

char *sel_sockname = NULL;                                                      //Socket filename usato per apertura della connessione
int socket_fd = -1;                                                             //File descriptor del socket file

int openConnection(const char *socketname, int msec, const struct timespec abstime);

int closeConnection(const char *sockname);

int openFile(const char *pathname, int flags);

int readFile(const char *pathname, void **buf, size_t *size);

int readNFiles(int n, const char *dirname);

int writeFile(const char *pathname, const char *dirname);

int appendToFile(const char *pathname, void *buf, size_t size, const char *dirname);

int lockFile(const char *pathname);

int unlockFile(const char *pathname);

int closeFile(const char *pathname);

int removeFile(const char *pathname);


int openConnection(const char *socketname, int msec, const struct timespec abstime) {
    int opened = 0;
    struct timespec acttime;
    struct sockaddr_un sock_addr;

    //Verifica se una connessione è già aperta
    if(sel_sockname != NULL) {
        errno = EISCONN;

        return -1;
    }

    socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    sock_addr.sun_family = AF_UNIX;

    strncpy(sock_addr.sun_path, socketname, UNIX_PATH_MAX);

    //Tenta di aprire una connessione con il server fino al raggiungimento di abstime
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

    sel_sockname = malloc(strlen(socketname) * sizeof(char));

    //Verifica se la connessione è stata aperta con successo
    if(opened) {
        printf("Connessione aperta con il server\n");

        strcpy(sel_sockname, socketname);

        return 0;
    } else {
        //Si è verificato un problema, errno è impostato da connect

        free(sel_sockname);
        sel_sockname = NULL;

        return -1;
    }
}

int closeConnection(const char *sockname) {
    //Verifica se la connessione non è già stata chiusa
    if(sel_sockname == NULL) {
        errno = ENOTCONN;

        return -1;
    }

    //Verifica se il sockname passato come argomento è lo stesso di quello su cui è aperta la connessione
    if(strcmp(sockname, sel_sockname) == 0) {
        errno = EINVAL;

        return -1;
    }

    if(close(socket_fd)) {
        return -1;
    }

    free(sel_sockname);

    sel_sockname = NULL;
    socket_fd = -1;
}

int openFile(const char *pathname, int flags) {
    char *response_m;

    if(sel_sockname == NULL) {
        errno = ENOTCONN;

        return -1;
    }

    response_m = malloc(RESPONSE_BUFF_SIZE * sizeof(char));

    if(write(socket_fd, "OPENFILE,./, O_CREATE | O_LOCK", 31) == -1) {
        return -1;
    }

    if(read(socket_fd, response_m, sizeof(response_m)) == -1) {
        return -1;
    }

    if(strcmp(response_m, SUCCESS) == 0) {
        fprintf(stderr, "aperto\n");
        return 0;
    } else {
        //TODO: Aggiungere impostazione di errno
        return -1;
    }

    free(response_m);

    return 0;
}

int readFile(const char *pathname, void **buf, size_t *size) {

}

int readNFiles(int n, const char *dirname) {

}

int writeFile(const char *pathname, const char *dirname) {

}

int appendToFile(const char *pathname, void *buf, size_t size, const char *dirname) {

}

int lockFile(const char *pathname) {

}

int unlockFile(const char *pathname) {

}

int closeFile(const char *pathname) {

}

int removeFile(const char *pathname) {

}
