#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <time.h>
#include <stdlib.h>
#include <limits.h>

#include "api.h"

#define SOCKETNAME ""
#define TIMEOUT 500
#define ABSTIME_OFFSET 5
#define O_CREATE 1
#define O_LOCK 2

#define F_BIT 2048
#define UPPER_D_BIT 1024
#define LOWER_W_BIT 512
#define UPPER_W_BIT 256
#define LOWER_R_BIT 128
#define UPPER_R_BIT 64
#define LOWER_D_BIT 32
#define T_BIT 16
#define L_BIT 8
#define U_BIT 4
#define C_BIT 2
#define P_BIT 1

#define UNIX_PATH_MAX 108

//Legge il contenuto della directory 'dirname' e invia i file contentuti al suo interno al server fino ad un massimo di n file
//return il numero di byte scritti in caso di successo, -1 altrimenti
int lower_w_fun(char *dirname, int n, char *save_dirname);

//Legge i filename presenti in 'files' e invia i corrispondenti file al server
int upper_w_fun(char *files, char *save_dirname);

int main(int argc, char *argv[]) {
    int p = 0;
    int i;
    struct timespec abstime;
    int arg_bit_mask = 0;
    char *sockname;
    char *D_dirname;
    char *d_dirname;
    char *w_dirname;
    long n;
    int success;
    char *error;

    if(argc < 2) {
        return EINVAL;
    }

    i = 1;
    while(i < argc) {
        if(strcmp(argv[1], "-h") == 0) {
            printf("NAME\n\t filestorage - send one or more requests to the file storage server\n");
            printf("SYNOPSIS\n\t filestorage ARGUMENT [FILE]...\n");
            printf("DESCRIPTION\n\t-f filename\tselect the name of AF_UNIX socket to connect to\n\t-w dirname[,n=0]\tsend to server all files in 'dirname', if 'dirname' contains other directories,\n\t\t\t\tthese will be visited recursively until n files had read, with n=0 there is no upper limit\n\t-W file1[,file2]\tlist of files to be written to the server separated by','\n");

            return 0;
        } else if(strcmp(argv[i], "-f") == 0) {
            if(arg_bit_mask & F_BIT) {
                printf("-f can be used once only, use -h for help\n");

                return -1;
            }

            if(argc > i+1 && argv[i+1][0] != '-') {
                arg_bit_mask = arg_bit_mask | F_BIT;
                sockname = argv[i+1];

                i += 2;
            } else {
                printf("Socket filename not defined, use -h for help\n");

                return -1;
            }
        } else if(strcmp(argv[i], "-D") == 0) {
            if(argc > i+1 && argv[i+1][0] != '-') {
                i += 2;
            } else {
                i++;
            }
        } else if(strcmp(argv[i], "-w") == 0) {
            arg_bit_mask = arg_bit_mask | LOWER_W_BIT;

            i += 2;
        } else if(strcmp(argv[i], "-W") == 0) {
            arg_bit_mask = arg_bit_mask | UPPER_W_BIT;

            i += 2;
        } else if(strcmp(argv[i], "-r") == 0) {
            arg_bit_mask = arg_bit_mask | LOWER_R_BIT;

            i += 2;
        } else if(strcmp(argv[i], "-R") == 0) {
            arg_bit_mask = arg_bit_mask | UPPER_R_BIT;

            if(argv[i+1][0] != '-') {
                i += 2;
            } else {
                i++;
            }
        } else if(strcmp(argv[i], "-d") == 0) {

            arg_bit_mask = arg_bit_mask | LOWER_D_BIT;

            i += 2;
        } else if(strcmp(argv[i], "-t") == 0) {
            arg_bit_mask = arg_bit_mask | T_BIT;

            i += 2;
        } else if(strcmp(argv[i], "-l") == 0) {
            arg_bit_mask = arg_bit_mask | L_BIT;

            i += 2;
        } else if(strcmp(argv[i], "-u") == 0) {
            arg_bit_mask = arg_bit_mask | U_BIT;

            i += 2;
        } else if(strcmp(argv[i], "-c") == 0) {
            arg_bit_mask = arg_bit_mask | C_BIT;

            i += 2;
        } else if(strcmp(argv[i], "-p") == 0) {
            if(arg_bit_mask & P_BIT) {
                printf("-p can be used once only, use -h for help");

                return -1;
            }

            arg_bit_mask = arg_bit_mask | P_BIT;

            i++;
        } else {
            printf("Invalid argument");

            i++;
        }
    }

    //Verifica se il socket filename è stato definito correttamente
    if(!(arg_bit_mask & F_BIT)) {
        printf("Socket filename not defined, use -h for help\n");

        return -1;
    }

    //Usa l'api per aprire una connessione con il server
    abstime.tv_sec = time(NULL) + ABSTIME_OFFSET;
    if(openConnection(sockname, TIMEOUT, abstime)) {
        perror("Opening connection with server");
    }

    i = 1;
    while(i < argc) {
        if(strcmp(argv[i], "-f") == 0) {
            //Verifica se è richiesta la modalità verbose
            if(arg_bit_mask & P_BIT) {
                printf("-f %s: Successful\n", sockname);
            }

            i += 2;
        } else if(strcmp(argv[i], "-w") == 0) {
            int result;
            success = 0;
            //Verifica se la directory di cui caricare i file sul server è specificata
            if(argc > i+1 && argv[i+1][0] != '-') {
                //Directory specificata

                //Verifica se n è specificato
                if(strstr(argv[i+1], ",") == NULL) {
                    //n non specificato
                    w_dirname = argv[i+1];

                    n = -1;
                } else {
                    //n specificato
                    char *n_string;

                    w_dirname = strtok(argv[i+1], ",");

                    n_string = strtok(NULL, ",");

                    if((n = strtol(n_string + 2, NULL, 10)) == LONG_MIN || n == LONG_MAX) {
                        n = -2;
                    }

                    if(n < 1) {
                        error = "Invalid value for n";
                        n = -2;
                    }
                }

                if(n != -2) {
                    //Verifica se l'argomento -D è specificato
                    if(argc > i+2 && strcmp(argv[i+2], "-D") != 0) {
                        //-D non specificato
                        D_dirname = NULL;

                        i += 3;
                    } else if(argc > i+2){
                        //-D specificato

                        //Verifica se la directory per l'argomento -D è specificata
                        if(argc > i+3 && argv[i+3][0] != '-') {

                            //Directory specificata
                            D_dirname = argv[i+3];

                            i += 4;

                            //Elabora tutti i file da inviare al server
                            if((result = lower_w_fun(w_dirname, n, D_dirname)) != -1) {
                                success = 1;
                            } else {
                                perror("Writing files to server: ");
                            }

                        } else {
                            //Directory non specificata
                            error = "Directory for -D argument not defined";

                            i += 3;
                        }
                    } else {
                        i += 2;

                        if((lower_w_fun(w_dirname, n, D_dirname)) != -1) {
                            success = 1;
                        } else {
                            perror("Writing files to server");
                        }

                    }
                } else {
                    error = "Invalid value for n";

                    i += 2;
                }
            } else {
                //Directory non specificata
                error = "Directory path not defined";

                i++;
            }

            if(arg_bit_mask & P_BIT) {
                if(!success) {
                    printf("-w %s: Unsuccessful: %s, use -h for help\n", w_dirname, error);
                } else {
                    printf("-w %s: Successfull byte written: %dbytes\n", w_dirname, result);
                }
            }
        } else if(strcmp(argv[i], "-W") == 0) {

        } else if(strcmp(argv[i], "-r") == 0) {

        } else if(strcmp(argv[i], "-R") == 0) {

        } else if(strcmp(argv[i], "-d") == 0) {

        } else if(strcmp(argv[i], "-t") == 0) {

        } else if(strcmp(argv[i], "-l") == 0) {

        } else if(strcmp(argv[i], "-u") == 0) {

        } else if(strcmp(argv[i], "-c") == 0) {

        } else if(strcmp(argv[i], "-p") == 0) {
            i++;
        } else {
            printf("Invalid argument");

            i++;
        }
    }

    //Usa l'api per chiudere la connessione con il server
    if(closeConnection(SOCKETNAME)) {
        perror("Closing connection with server");
    }

}

int lower_w_fun(char *dirname, int n, char *save_dirname) {
    DIR *dir;
    struct dirent *dir_cont;

    //Verifica se bisogna inviare altri file
    if(n == 0) {
        return 0;
    }

    //Apre la directory passata come argomento
    if((dir = opendir(dirname)) == NULL) {
        return -1;
    }

    //Scansiona il contenuto della directory
    while((dir_cont = readdir(dir)) != NULL) {
        //Verifica se dir_cont è un file regolare
        if(dir_cont->d_type == DT_REG) {
            //Usa l'api per aprire il file o crearlo
            if(openFile(dir_cont->d_name, O_CREATE | O_LOCK) != 0) {
                return -1;
            }

            //Usa l'api per scrivere il contenuto del file
            if(writeFile(dir_cont->d_name, save_dirname)){
                //C'è stato un errore nella scrittura elimina il file creato usando l'api
                removeFile(dir_cont->d_name);

                return -1;
            } else {
                //Usa l'api per chiudere il file
                if(closeFile(dir_cont->d_name)) {
                    return -1;
                }
            }
        }
    }

    rewinddir(dir);

    while((dir_cont = readdir(dir)) != NULL) {
        //Verifica se dir_cont è una directory
        if(dir_cont->d_type == DT_DIR) {
            if(lower_w_fun(dir_cont->d_name, n, save_dirname)) {
                return -1;
            }
        }
    }

    if(closedir(dir)) {
        return -1;
    }

    return 0;
}
