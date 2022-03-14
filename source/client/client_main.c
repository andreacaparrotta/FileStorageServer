#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <time.h>
#include <stdlib.h>

#include "api.h"

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

//Legge il contenuto della directory 'dirname' e invia i file contentuti al suo interno al server fino ad un massimo di n file
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
    int n;

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
                printf("-f can be used once only, use -h to see usage");

                return -1;
            }

            arg_bit_mask = arg_bit_mask | F_BIT;

            i += 2;
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
                printf("-p can be used once only, use -h to see usage");

                return -1;
            }

            arg_bit_mask = arg_bit_mask | P_BIT;

            i++;
        } else {
            printf("Invalid argument");

            i++;
        }
    }

    fprintf(stderr, "IMPOSTAZIONE BIT MASK AVVENUTA CON SUCCESSO\n\n");

    i = 1;
    while(i < argc) {
        if(strcmp(argv[i], "-f") == 0) {
            //Verifica se il socket filename è specificato
            if(argc > i+1 && argv[i+1][0] != '-') {
                //Socket filename specificato, imposta il sockname
                sockname = argv[i+1];

                //Verifica la richiesta di modalità verbose
                if(arg_bit_mask & P_BIT) {
                    printf("Socket filename setted to: %s\n", sockname);
                }

                i += 2;
            } else {
                //Socket filename non specificato
                printf("Socket filename not defined, please use -h for help\n");

                i++;
            }
        } else if(strcmp(argv[i], "-w") == 0) {
            //Verifica se la directory di cui caricare i file sul server è specificata
            if(argc > i+1 && argv[i+1][0] != '-') {
                //Directory specificata

                //Verifica se n è specificato
                if(strstr(argv[i+1], ",") != NULL) {
                    //n non specificato
                    w_dirname = argv[i+1];

                    n = -1;
                } else {
                    //n specificato

                    w_dirname = strtok(argv[i+1], ",");
                    n = (int)strtol(strtok(NULL, ",") + 3, NULL, 10);
                }

                //Verifica se l'argomento -D è specificato
                if(argc > i+2 && strcmp(argv[i+2], "-D") != 0) {
                    //-D non specificato
                    D_dirname = NULL;

                    i += 3;
                } else {
                    //-D specificato

                    //Verifica se la directory per l'argomento -D è specificata
                    if(argc > i+3 && argv[i+3][0] != '-') {
                        D_dirname = argv[i+3];

                        i += 4;
                    } else {
                        printf("Directory for -D argument not defined, please use -h for help\n");

                        i += 3;
                    }
                }

                if(lower_w_fun(w_dirname, n, D_dirname)) {
                    perror("Writing files from directory: ");
                }
            } else {
                //Directory non specificata
                printf("Directory path not defined, please use -h for help\n");

                i++;
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
}
