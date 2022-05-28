#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <limits.h>

#include "definitions.h"
#include "api.h"

#define TIMEOUT 500
#define ABSTIME_OFFSET 5

#define F_BIT 4
#define T_BIT 2
#define P_BIT 1

#define UNIX_PATH_MAX 108

/*
 * Esegue le operazioni necessarie per scrivere un file sul server
 * Parametri: 
 *      pathname: il filename del file da scrivere sul server
 *      dirname: il percorso della directory in cui salvare eventuali file espulsi dal server, se NULL allora i file inviati dal server vengono ignorati
 *      timeout: il tempo che intercorre tra l'invio di due richieste al server
 * Errno:
 *      ENOENT: se la directory per il salvataggio non esiste
 *      EPERM: se l'utente non ha i permessi di accesso alla directory per il salvataggio
 * Ritorna: 0 in caso di successo, -1 in caso di errore
 */
int write_file(char *pathname, char *dirname, long timeout);

/*
 * Legge il contenuto della directory "dirname" e invia i file contenuti nella directory e nelle sue sottodirectory al server fino a raggiungere n file
 * se n non è definito oppure n == 0 allora vengono inviati tutti i file
 * Parametri:
 *      dirname: il percorso della directory di cui scrivere i file sul server
 *      n: il numero massimo di file da inviare al server     
 *      part_n: indica il numero massimo rimanente di file da scrivere per invocazioni ricorsive della funzione
 *      save_dirname: il percorso della directory in cui salvare eventuali file espulsi dal server, se NULL allora i file inviati dal server vengono ignorati
 *      timeout: il tempo che intercorre tra l'invio di due richieste al server
 *      p: indica se è richiesta la modalità verbose
 * Ritorna: 0 in caso di successo, -1 in caso di errore 
 */
int lower_w_fun(char *dirname, int n, int part_n, char *save_dirname, long timeout, int p);

/*
 * Invia al server i file con filename definiti in files
 * Parametri:
 *      files: contiene i filename dei file da scrivere sul server separati da una virgola
 *      save_dirname: il percorso della directory in cui salvare eventuali file espulsi dal server, se NULL allora i file inviati dal server vengono ignorati
 *      timeout: il tempo che intercorre tra l'invio di due richieste al server
 *      p: indica se è richiesta la modalità verbose
 * Ritorna: 0 in caso di successo, -1 in caso di errore 
 */
int upper_w_fun(char *files, char *save_dirname, long timeout, int p);

/*
 * Richiede al server la lettura dei file con i filename definiti in files
 * Parametri:
 *      files: i filename dei file da leggere
 *      dirname: il percorso della directory in cui salvare i file letti dal server, se NULL allora i file inviati dal server non vengono salvati
 *      timeout: il tempo che intercorre tra l'invio di due richieste al server
 *      p: indica se è richiesta la modalità verbose
 * Ritorna: 0 in caso di successo, -1 in caso di errore 
 */
int lower_r_fun(char *files, char *dirname, long timeout, int p);

/*
 * Richiede la lettura di al più n file qualsiasi dal server
 * Parametri:
 *      n: il numero massimo di file da leggere, se n == 0 allora si richiede di leggere tutti i file
 *      dirname: il percorso della directory in cui salvare i file letti dal server, se NULL allora i file inviati dal server non vengono salvati
 *      p: indica se è richiesta la modalità verbose
 * Ritorna: 0 in caso di successo, -1 in caso di errore 
 */
int upper_r_fun(int n, char *dirname, int p);

/*
 * Richiede al server l'ottenimento della lock sui file i cui filename sono contenuti in files
 * Parametri:
 *      files: i filename dei file su cui ottentere la lock
 *      timeout: il tempo che intercorre tra l'invio di due richieste al server
 *      p: indica se è richiesta la modalità verbose
 * Ritorna: 0 in caso di successo, -1 in caso di errore 
 */
int lower_l_fun(char *files, long timeout, int p);

/*
 * Richiede al server il rilascio della lock sui file i cui filename sono contenuti in files
 * Parametri:
 *      files: i filename dei file su cui rilasciare la lock
 *      timeout: il tempo che intercorre tra l'invio di due richieste al server
 *      p: indica se è richiesta la modalità verbose
 * Ritorna: 0 in caso di successo, -1 in caso di errore 
 */
int lower_u_fun(char *files, long timeout, int p);

/*
 * Richiede al server l'eliminazione dei file i cui filename sono contenuti in files
 * Parametri:
 *      files: i filename dei file da eliminare
 *      timeout: il tempo che intercorre tra l'invio di due richieste al server
 *      p: indica se è richiesta la modalità verbose
 * Ritorna: 0 in caso di successo, -1 in caso di errore 
 */
int lower_c_fun(char *files, long timeout, int p);

int main(int argc, char *argv[]) {
    int i;
    struct timespec abstime;
    int arg_bit_mask = 0;

    char *sockname;
    char *D_dirname;
    char *d_dirname;
    char *w_dirname;
    char *filenames;

    long n;
    int part_n;
    int timeout = 0;
    int all_set;

    if(argc < 2) {
        return EINVAL;
    }

    i = 1;
    while(i < argc) {
        if(strcmp(argv[1], "-h") == 0) {
            printf("NAME\n\t filestorage - invia una o più richieste ad un file storage server\n");
            printf("SYNOPSIS\n\t ./filestorage COMMANDS [PATH[,PATH[,...]][,n=VALUE]] [OPTIONS] ... \n");
            printf("DESCRIPTION\n\t");
            printf("Filestorage è un client per l'interazione con un server filestorage\n");
            printf("OPTIONS\n\t");
            printf("-f filename\tSeleziona il nome del socket AF_UNIX a cui connettersi\n\t");
            printf("-D pathname\tDefinisce il percorso della directory in cui salvare i file espulsi dal server, deve essere usato subito dopo i comandi -w e -W\n\t");
            printf("-d pathname\tDefinisce il percorso della directory in cui salvare i file letti dal server, deve essere usato subito dopo i comandi -r e -R\n\t");
            printf("-t time\t\tDefinisce il tempo che intercorre tra l'invio di due richieste successive al server\n\t");
            printf("-p\t\tAbilita la modalità verbose\n");
            printf("COMMANDS\n\t");
            printf("-w dirname[,n=0]\tInvia al server tutti i file contenuti in 'dirname', se 'dirname' contiene altre directory,\n\t\t\t\tqueste saranno visitate ricorsivamente fino a scrivere n file, se n=0 non c'è un limite superiore\n\t");
            printf("-W file1[,file2[,...]]\tInvia al server tutti i file definiti\n\t");
            printf("-r file1[,file2[,...]]\tLegge dal server tutti i file definiti\n\t");
            printf("-R [n=0]\t\tLegge al più n file dal server, se n non è definito oppure se n è uguale a 0 allora vengono letti tutti i file dal server\n\t");
            printf("-l file1[,file2[,...]]\tRichiede la lock su tutti i file definiti, se la lock è già posseduta da un altro client allora l'operazione fallisce\n\t");
            printf("-u file1[,file2[,...]]\tRichiede il rilascio della lock su tutti i file definiti, se il client non possiede la lock sul file l'operazione fallisce\n\t");
            printf("-c file1[,file2[,...]]\tElimina dal server tutti i file definiti\n");

            return 0;
        } else if(strcmp(argv[i], "-f") == 0) {
            if(arg_bit_mask & F_BIT) {
                printf("-f: Errore, può essere definito solo una volta, usa -h per aiuto\n");

                return -1;
            }

            if(argc > i+1 && argv[i+1][0] != '-') {
                arg_bit_mask = arg_bit_mask | F_BIT;
                sockname = argv[i+1];

                i += 2;
            } else {
                printf("-f: Errore, il filename del socket non è definito, usa -h per aiuto\n");

                return -1;
            }
        } else if(strcmp(argv[i], "-t") == 0) {
            if(argc > i+1 && argv[i+1][0] != '-') {
                arg_bit_mask = arg_bit_mask | T_BIT;

                errno = 0;
                timeout = strtol(argv[i+1], NULL, 10) * 1000;

                if(errno == ERANGE) {
                    printf("-t: Errore, il valore per il timeout non è valido, usa -h per aiuto\n");

                    return -1;
                }

                i += 2;
            } else {
                printf("-t: Errore, il valore per il timeout non è definito, usa -h per aiuto\n");

                return -1;
            }
        } else if(strcmp(argv[i], "-p") == 0) {
            if(arg_bit_mask & P_BIT) {
                printf("-p: Errore, può essere usato solo una volta, usa -h per aiuto");

                return -1;
            }

            arg_bit_mask = arg_bit_mask | P_BIT;
            print_upper_r = 1;

            i++;
        } else {
            i++;
        }
    }

    // Verifica se il socket filename è stato definito correttamente
    if(!(arg_bit_mask & F_BIT)) {
        if(arg_bit_mask & P_BIT) {
            printf("-f: Errore, il filename del socket non è definito, usa -h per aiuto\n");
        }

        return -1;
    }

    // Usa l'api per aprire una connessione con il server
    abstime.tv_sec = time(NULL) + ABSTIME_OFFSET;
    if(openConnection(sockname, TIMEOUT, abstime)) {
        if(arg_bit_mask & P_BIT) {
            perror("-f");
        }
    } else {
        i = 1;
        while(i < argc) {
            if(timeout != 0) {
                usleep(timeout);
            }

            if(strcmp(argv[i], "-f") == 0) {
                if(arg_bit_mask & P_BIT) {
                    printf("-f %s: Successo\n", sockname);
                }

                i += 2;
            } else if(strcmp(argv[i], "-w") == 0) {
                // Verifica se la directory di cui caricare i file sul server è definita
                if(argc > i+1 && argv[i+1][0] != '-') {
                    // Directory definita

                    // Verifica se n è definito
                    if(strstr(argv[i+1], ",") == NULL) {
                        // n non definito
                        w_dirname = argv[i+1];

                        n = 0;
                        part_n = 1;

                        all_set = 1;
                    } else {
                        // n definito
                        char *n_string;

                        w_dirname = strtok(argv[i+1], ",");

                        n_string = strtok(NULL, ",");

                        if(n_string == NULL) {
                            if(arg_bit_mask & P_BIT) {
                                printf("-w: Errore, atteso argomento 'n' dopo il carattere ',', usa -h per aiuto\n");
                            }

                            all_set = 0;

                            i += 2;
                        } else {
                            if((n = strtol(n_string + 2, NULL, 10)) == LONG_MIN || n == LONG_MAX) {
                                all_set = 0;
                            }

                            if(n == 0) {
                                part_n = 1;
                            } else {
                                part_n = n;
                            }

                            if(n < 0) {
                                if(arg_bit_mask & P_BIT) {
                                    printf("-w: Errore, valore non valido per n, usa -h per aiuto\n");
                                }

                                all_set = 0;
                            } else {
                                all_set = 1;
                            }
                        }
                    }
                } else {
                    // Directory non definita
                    if(arg_bit_mask & P_BIT) {
                        printf("-w: Errore, directory non definita, usa -h per aiuto\n");
                    }

                    if(argc > i+1 && strcmp(argv[i+1], "-D") == 0) {
                        if(argc > i+2 && argv[i+2][0] != '-') {
                            i += 3;
                        } else {
                            i += 2;
                        }
                    } else {
                        i++;
                    }

                    all_set = 0;
                }

                if(all_set) {
                    // Verifica se l'argomento -D è definito
                    if(argc > i+2 && strcmp(argv[i+2], "-D") == 0) {
                        // -D definito

                        // Verifica se la directory per l'argomento -D è definita
                        if(argc > i+3 && argv[i+3][0] != '-') {

                            // Directory definita
                            D_dirname = argv[i+3];

                            i += 4;

                            all_set = 1;
                        } else {
                            // Directory non definita
                            printf("-w: Errore, la directory per l'argomento -D non è definita, usa -h per aiuto\n");

                            i += 3;

                            all_set = 0;
                        }
                    } else {
                        // -D non definito
                        D_dirname = NULL;

                        i += 2;

                        all_set = 1;
                    }
                }

                if(all_set) {
                    lower_w_fun(w_dirname, n, part_n, D_dirname, timeout, arg_bit_mask & P_BIT);
                }
            } else if(strcmp(argv[i], "-W") == 0) {
                //Verifica se i file da caricare sul server sono definiti
                if(argc > i+1 && argv[i+1][0] != '-') {
                    //File definiti
                    filenames = argv[i+1];

                    all_set = 1;
                } else {
                    //File non definiti
                    printf("-W: Errore, file da scrivere sul server non definiti, usa -h per aiuto\n");

                    if(argc > i+1 && strcmp(argv[i+1], "-D") == 0) {
                        if(argc > i+2 && argv[i+2][0] != '-') {
                            i += 3;
                        } else {
                            i += 2;
                        }
                    } else {
                        i++;
                    }

                    all_set = 0;
                }

                if(all_set) {
                    //Verifica se l'argomento -D è definito
                    if(argc > i+2 && strcmp(argv[i+2], "-D") == 0) {
                        //-D definito

                        //Verifica se la directory per l'argomento -D è definita
                        if(argc > i+3 && argv[i+3][0] != '-') {

                            //Directory definita
                            D_dirname = argv[i+3];

                            i += 4;

                            all_set = 1;
                        } else {
                            //Directory non definita
                            printf("-W: Errore, la directory per l'argomento -D non è definita, usa -h per aiuto\n");

                            i += 3;

                            all_set = 0;
                        }
                    } else {
                        //-D non definito
                        D_dirname = NULL;

                        i += 2;

                        all_set = 1;
                    }
                }

                if(all_set) {
                    upper_w_fun(filenames, D_dirname, timeout, arg_bit_mask & P_BIT);
                }
            } else if(strcmp(argv[i], "-D") == 0) {
                printf("-D può essere usato solo dopo -w o -W, usa -h per aiuto\n");

                if(argc > i + 1 && argv[i + 1][0] != '-') {
                    i += 2;
                } else {
                    i++;
                }
            } else if(strcmp(argv[i], "-r") == 0) {
                char *filenames;

                if(argc > i + 1 && argv[i+1][0] != '-') {
                    // File definiti

                    filenames = argv[i+1];

                    all_set = 1;
                } else {
                    //File non definiti
                    printf("-r: Errore, file da leggere non definiti, usa -h per aiuto\n");

                    if(argc > i+1 && strcmp(argv[i+1], "-d") == 0) {
                        if(argc > i+2 && argv[i+2][0] != '-') {
                            i += 3;
                        } else {
                            i += 2;
                        }
                    } else {
                        i++;
                    }

                    all_set = 0;
                }

                if(all_set){
                    //Verifica se l'argomento -d è definito
                    if(argc > i+2 && strcmp(argv[i+2], "-d") == 0) {
                        //-d definito

                        //Verifica se la directory per l'argomento -D è definita
                        if(argc > i+3 && argv[i+3][0] != '-') {

                            //Directory definita
                            d_dirname = argv[i+3];

                            i += 4;
                            all_set = 1;
                        } else {
                            //Directory non definita
                            printf("-r: Errore, la directory per l'argomento -d non è definita, usa -h per aiuto\n");

                            i += 3;

                            all_set = 0;
                        }
                    } else {
                        //-D non definito
                        d_dirname = NULL;

                        i += 2;

                        all_set = 1;
                    }
                }

                if(all_set) {
                    lower_r_fun(filenames, d_dirname, timeout, arg_bit_mask & P_BIT);
                }
            } else if(strcmp(argv[i], "-R") == 0) {
                int n;

                if(argc > i + 1 && argv[i+1][0] != '-') {
                    // Valore per n definito
                    
                    n = strtol(argv[i+1] + 2, NULL, 10);

                    all_set = 1;
                } else {
                    //File non definiti
                    n = 0;

                    all_set = 0;
                }

                if(all_set) {
                    //Verifica se l'argomento -d è definito
                    if(argc > i+2 && strcmp(argv[i+2], "-d") == 0) {
                        //-d definito

                        //Verifica se la directory per l'argomento -D è definita
                        if(argc > i+3 && argv[i+3][0] != '-') {

                            //Directory definita
                            d_dirname = argv[i+3];

                            i += 4;

                            all_set = 1;
                        } else {
                            //Directory non definita
                            printf("-R: Errore, la directory per l'argomento -d non è definita, usa -h per aiuto\n");

                            i += 3;

                            all_set = 0;
                        }
                    } else {
                        //-D non definito
                        d_dirname = NULL;

                        i += 2;

                        all_set = 1;
                    }
                } else {
                    //Verifica se l'argomento -d è definito
                    if(argc > i+1 && strcmp(argv[i+1], "-d") == 0) {
                        //-d definito

                        //Verifica se la directory per l'argomento -D è definita
                        if(argc > i+2 && argv[i+2][0] != '-') {

                            //Directory definita
                            d_dirname = argv[i+2];

                            i += 3;

                            all_set = 1;
                        } else {
                            //Directory non definita
                            printf("-R: Errore, la directory per l'argomento -d non è definita, usa -h per aiuto\n");

                            i += 2;

                            all_set = 0;
                        }
                    } else {
                        //-D non definito
                        d_dirname = NULL;

                        i += 1;

                        all_set = 1;
                    }
                }

                if(all_set) {
                    upper_r_fun(n, d_dirname, arg_bit_mask & P_BIT);
                }
            } else if(strcmp(argv[i], "-d") == 0) {
                printf("-d: Errore, -d può essere usato solo dopo -r o -R, usa -h per aiuto\n");

                break;
            } else if(strcmp(argv[i], "-t") == 0) {
                i += 2;
            } else if(strcmp(argv[i], "-l") == 0) {

                //Verifica se i file su cui ottenere la lock sono definiti
                if(argc > i+1 && argv[i+1][0] != '-') {
                    //File definiti
                    filenames = argv[i+1];

                    i += 2;

                    lower_l_fun(filenames, timeout, arg_bit_mask & P_BIT);
                } else {
                    //File non definiti
                    printf("-l: Errore, file non definiti, usa -h per aiuto\n");

                    i++;
                }
            } else if(strcmp(argv[i], "-u") == 0) {

                //Verifica se i file su cui ottenere la lock sono definiti
                if(argc > i+1 && argv[i+1][0] != '-') {
                    //File definiti
                    filenames = argv[i+1];

                    i += 2;

                    lower_u_fun(filenames, timeout, arg_bit_mask & P_BIT);
                } else {
                    //File non definiti
                    printf("-u: Errore, file non definiti, usa -h per aiuto\n");

                    i++;
                }
            } else if(strcmp(argv[i], "-c") == 0) {

                //Verifica se i file su cui ottenere la lock sono definiti
                if(argc > i+1 && argv[i+1][0] != '-') {
                    //File definiti
                    filenames = argv[i+1];

                    i += 2;

                    lower_c_fun(filenames, timeout, arg_bit_mask & P_BIT);
                } else {
                    //File non definiti
                printf("-c: Errore, file non definiti, usa -h per aiuto\n");

                    i++;
                }
            } else if(strcmp(argv[i], "-p") == 0) {
                i++;
            } else {
                printf("%s: Argomento non valido, usa -h per aiuto\n", argv[i]);

                i++;
            }
        }

        //Usa l'api per chiudere la connessione con il server
        if(closeConnection(sockname)) {
            perror("Chiudendo la connessione con il server");
        }
    }

    return 0;
}

int write_file(char *pathname, char *dirname, long timeout) {
    int opened = 0;
    int result = 0;

    if(dirname != NULL) {
        if(access(dirname, F_OK) == -1) {
            errno = ENOENT;

            return -1;
        } else if(access(dirname, R_OK | W_OK) == -1) {
            errno = EPERM;

            return -1;
        }

        set_dirname(dirname);
    }

    errno = 0;
    while(!opened) {
        if(openFile(pathname, O_CREATE | O_LOCK) == -1) {
            if(errno == EEXIST) {
                if(openFile(pathname, O_LOCK) == -1) {
                    result = -1;
                    opened = -1;
                } else {
                    removeFile(pathname);
                }
            } else {
                result = -1;
                opened = -1;
            }
        } else {
            opened = 1;
        }
    }

    if(opened == 1) {
        if(timeout != 0) {
            usleep(timeout);
        }

        //Usa l'api per scrivere il contenuto del file
        if(writeFile(pathname, dirname) == -1){
                        
            result = -1;
        }

        if(timeout != 0) {
            usleep(timeout);
        }

        //Usa l'api per chiudere il file
        if(closeFile(pathname) == -1) {
                        
            result = -1;
        }
    }

    if(dirname != NULL) {
        reset_dirname();
    }

    return result;
}

int lower_w_fun(char *dirname, int n, int part_n, char *save_dirname, long timeout, int p) {
    DIR *dir;
    FILE *file;
    struct dirent *dir_cont;
    
    char n_dirname[UNIX_PATH_MAX];
    char pathname[UNIX_PATH_MAX];
    char *abs_pathname;

    int file_size;
    int result;

    if(dirname == NULL) {
        if(p) {
            printf("-w: Errore, directory non definita, usa -h per aiuto\n");
        }
        
        return -1;
    }

    //Apre la directory passata come argomento
    if((dir = opendir(dirname)) == NULL) {
        if(p) {
            printf("-w: Errore, è avvenuto un errore durante l'apertura della directory\n");
        }

        return -1;
    }

    //Scansiona il contenuto della directory
    while((dir_cont = readdir(dir)) != NULL) {
        result = 0;

        //Verifica se dir_cont è un file regolare
        if(dir_cont->d_type == DT_REG) {
            if((strlen(dirname) + strlen(dir_cont->d_name) + 1) < UNIX_PATH_MAX) {
                strcpy(pathname, dirname);

                if(pathname[strlen(dirname) - 1] != '/') {
                    strcat(pathname, "/");
                }

                strcat(pathname, dir_cont->d_name);

                abs_pathname = realpath(pathname, NULL);

                // Verifica e apre il file pathname
                if((file = fopen(pathname, "r")) == NULL) {
                    result = -1;
                }

                if(result != -1) {
                    fseek(file, 0L, SEEK_END);
                    file_size = ftell(file);
                    fseek(file, 0L, SEEK_SET);
                    fclose(file);

                    errno = 0;
                    if(write_file(abs_pathname, save_dirname, timeout) == -1) {

                        result = -1;
                    }

                    if(n != 0) {
                        part_n--;
                    }
                }

                if(p) {
                    if(result != -1) {
                        printf("-w %s: Successo, %dbytes scritti\n", abs_pathname, file_size);
                    } else {
                        switch(errno) {
                            case EINVAL:
                                printf("-w %s: Errore, il file è vuoto oppure si è verificato un errore sconosciuto\n", abs_pathname);
                                break;
                            case ENOENT:
                                printf("-w %s: Errore, la directory definita con l'argomento -D non esiste\n", abs_pathname);
                                break;
                            case EPERM:
                                printf("-w %s: Errore, l'utente non ha i permessi di accesso alla directory definita con l'argomento -D\n", abs_pathname);
                                break;
                            case ENOMEM:
                                printf("-w %s: Errore, il file ha una dimensione eccessiva\n", abs_pathname);
                                break;
                        }

                        result = -1;
                    }
                }

                free(abs_pathname);

                //Verifica se bisogna inviare altri file
                if(part_n == 0) {
                    if(closedir(dir) != 0) {
                            
                        result = -1;
                    }

                    return result;
                }
            }
        }
    }

    rewinddir(dir);

    while((dir_cont = readdir(dir)) != NULL && part_n > 0) {

        //Verifica se dir_cont è una directory
        if(dir_cont->d_type == DT_DIR && strcmp(dir_cont->d_name, ".") != 0 && strcmp(dir_cont->d_name, "..") != 0) {
            if((strlen(dirname) + strlen(dir_cont->d_name) + 1) < UNIX_PATH_MAX) {
                strcpy(n_dirname, dirname);

                if(n_dirname[strlen(n_dirname) - 1] != '/') {
                    strcat(n_dirname, "/");
                }

                strcat(n_dirname, dir_cont->d_name);
            }

            part_n = lower_w_fun(n_dirname, n, part_n, save_dirname, timeout, p);
        }
    }

    if(closedir(dir) != 0) {
        result = -1;
    }

    return part_n;
}

int upper_w_fun(char *files, char *save_dirname, long timeout, int p) {
    FILE *file;
    char *pathname;
    char *abs_pathname;
    char *file_content;

    char *strtok_state;

    int result = 0;
    int file_size;

    pathname = strtok_r(files, ",", &strtok_state);
    while(pathname != NULL) {
        result = 0;

        abs_pathname = realpath(pathname, NULL);

        // Verifica e apre il file pathname
        if((file = fopen(pathname, "r")) == NULL) {
            if(p) {
                printf("-W %s: Errore, il file non esiste\n", pathname);
            }

            errno = 0;

            result = -1;
        }

        if(result != -1) {
            fseek(file, 0L, SEEK_END);
            file_size = ftell(file);
            fseek(file, 0L, SEEK_SET);

            file_content = malloc((file_size + 1) * sizeof(char));
            memset(file_content, 0, file_size + 1);
            fread(file_content, sizeof(char), file_size, file);
            fclose(file);

            if(write_file(abs_pathname, save_dirname, timeout) == -1) {
                result = -1;
            }

            free(file_content);
        }
        
        if(p) {
            if(result != -1) {
                printf("-W %s: Successo, %dbytes scritti\n", abs_pathname, file_size);
            } else {
                switch(errno) {
                    case ENOENT:
                        printf("-W %s: Errore, la directory definita con l'argomento -D non esiste\n", abs_pathname);
                        break;
                    case EPERM:
                        printf("-W %s: Errore, la lock sul file è posseduta da un altro utente oppure l'utente non ha i permessi di accesso alla directory definita con l'argomento -D\n", abs_pathname);
                        break;
                }
            }
        }

        free(abs_pathname);

        pathname = strtok_r(NULL, ",", &strtok_state);
    }

    return result;
}

int lower_r_fun(char *files, char *dirname, long timeout, int p) {
    FILE *file;

    char *pathname;                                     // pathname ottenuto dal pathname inserito dall'utente
    char *abs_pathname;                                 // pathname assoluto del pathname inserito dall'utente
    char saved_pathname[UNIX_PATH_MAX];                 // pathname in cui salvare il file letto dal server
    char *saved_filename;                               // nome con cui salvare il file nella directory di salvataggio

    char *strtok_state;

    char *content;
    size_t content_size;

    int result = 0;

    if(dirname != NULL) {
        if(access(dirname, F_OK) == -1) {
            if(p) {
                printf("-r: Errore, la directory in cui salvare i file non esiste\n");
            }

            return -1;
        } else if(access(dirname, R_OK | W_OK) == -1) {
            if(p) {
                printf("-r: Errore, l'utente non ha i permessi di accesso alla directory in cui salvare i file\n");
            }

            return -1;
        }
    }

    pathname = strtok_r(files, ",", &strtok_state);
    while(pathname != NULL) {
        abs_pathname = realpath(pathname, NULL);

        if(abs_pathname == NULL) {
            abs_pathname = malloc((UNIX_PATH_MAX + 1) * sizeof(char));
            getcwd(abs_pathname, UNIX_PATH_MAX + 1);

            strcat(abs_pathname, "/");
            if(pathname[0] == '.' && pathname[1] == '/') {
                strcat(abs_pathname, pathname + 2);
            } else {
                strcat(abs_pathname, pathname);
            }
        }

        if(openFile(abs_pathname, O_LOCK) == -1) {
            if(p) {
                printf("-r %s: Errore, il file non esiste\n", abs_pathname);
            }
            result = -1;
        } else {

            if(timeout != 0) {
                usleep(timeout);
            }

            //Usa l'api per scrivere il contenuto del file
            if(readFile(abs_pathname, (void **)&content, &content_size) == -1){

                result = -1;
            }

            if(result != -1 && dirname != NULL) {
                strcpy(saved_pathname, dirname);

                if(saved_pathname[strlen(saved_pathname) - 2] != '/') {
                    strcat(saved_pathname, "/");
                }

                saved_filename = strrchr(abs_pathname, '/') + 1;
                strcat(saved_pathname, saved_filename);

                if((file = fopen(saved_pathname, "w")) == NULL) {
                    if(p) {
                        perror("Aprendo il file");
                    }
                } else {
                    if(fwrite(content, sizeof(char), content_size, file) == 0) {
                        if(!feof(file)) {
                            if(p) {
                                printf("-r %s: Errore, errore sconosciuto durante il salvataggio del file in locale\n", abs_pathname);
                            }
                        }
                    }

                    fclose(file);
                }
            }

            if(p) {
                if(result != -1) {
                    printf("-r %s: Successo, %.33s\n", abs_pathname, content);

                    free(content);
                } else {
                    switch(errno) {
                        case ENOENT:
                            printf("-r %s: Errore, il file non esiste\n", abs_pathname);
                            break;
                        case EPERM:
                            printf("-r %s: Errore, la lock sul file è posseduta da un altro utente\n", abs_pathname);
                            break;
                    }
                }
            }

            if(timeout != 0) {
                usleep(timeout);
            }

            //Usa l'api per chiudere il file
            if(closeFile(abs_pathname) == -1) {

                result = -1;
            }
        }

        free(abs_pathname);
        
        pathname = strtok_r(NULL, ",", &strtok_state);
    }


    return 0;
}

int upper_r_fun(int n, char *dirname, int p) {
    if(dirname != NULL) {
        if(access(dirname, F_OK) == -1) {
            errno = 0;
            
            if(p) {
                printf("-R: Errore, la directory in cui salvare i file non esiste\n");
            }
            
            return -1;
        } else if(access(dirname, R_OK | W_OK) == -1) {
            errno = 0;

            if(p) {
                printf("-R: Errore, l'utente non ha i permessi di accesso alla directory in cui salvare i file\n");
            }

            return -1;
        }
    }
    
    errno = 0;
    if(p) {
        set_p();
    }

    return readNFiles(n, dirname);
}

int lower_l_fun(char *files, long timeout, int p) {
    char *pathname;
    char *abs_pathname;

    if(files == NULL) {

        return -1;
    }

    pathname = strtok(files, ",");
    while(pathname != NULL) {
        abs_pathname = realpath(pathname, NULL);

        if(abs_pathname == NULL) {
            abs_pathname = malloc((UNIX_PATH_MAX + 1) * sizeof(char));
            getcwd(abs_pathname, UNIX_PATH_MAX + 1);

            strcat(abs_pathname, "/");
            if(pathname[0] == '.' && pathname[1] == '/') {
                strcat(abs_pathname, pathname + 2);
            } else {
                strcat(abs_pathname, pathname);
            }
        }

        if(lockFile(abs_pathname) == -1) {
            if(p) {
                switch(errno) {
                    case EINVAL:
                        printf("-l %s: Errore, un errore sconosciuto è avvenuto, riprova più tardi\n", abs_pathname);

                        break;
                    case ENOENT:
                        printf("-l %s: Errore, il file non esiste, verifica il filename e riprova\n", abs_pathname);

                        break;
                    case EPERM:
                        printf("-l %s: Errore, la lock è posseduta da un altro utente, riprova più tardi\n", abs_pathname);

                        break;
                }
            }
        } else if(p){
            printf("-l %s: Successo\n", pathname);
        } 

        pathname = strtok(NULL, ",");

        free(abs_pathname);
    }

    return 0;
}

int lower_u_fun(char *files, long timeout, int p) {
    char *pathname;
    char *abs_pathname;

    if(files == NULL) {

        return -1;
    }

    pathname = strtok(files, ",");
    while(pathname != NULL) {
        abs_pathname = realpath(pathname, NULL);

        if(abs_pathname == NULL) {
            abs_pathname = malloc((UNIX_PATH_MAX + 1) * sizeof(char));
            getcwd(abs_pathname, UNIX_PATH_MAX + 1);

            strcat(abs_pathname, "/");
            if(pathname[0] == '.' && pathname[1] == '/') {
                strcat(abs_pathname, pathname + 2);
            } else {
                strcat(abs_pathname, pathname);
            }
        }

        if(unlockFile(abs_pathname) == -1) {
            if(p) {
                switch(errno) {
                    case EINVAL:
                        printf("-u %s: Errore, un errore sconosciuto è avvenuto, riprova più tardi\n", abs_pathname);

                        break;
                    case ENOENT:
                        printf("-u %s: Errore, il file non esiste, verifica il filename e riprova\n", abs_pathname);

                        break;
                    case EPERM:
                        printf("-c %s: Errore, non possiedi la lock su questo file\n", abs_pathname);
                        break;
                }
            } 
        } else if(p){
            printf("-u %s: Successo\n", pathname);
        }

        if(timeout != 0) {
            usleep(timeout);
        }

        pathname = strtok(NULL, ",");

        free(abs_pathname);
    }

    return 0;
}

int lower_c_fun(char *files, long timeout, int p) {
    char *pathname;
    char *abs_pathname;

    int result;

    if(files == NULL) {

        return -1;
    }

    pathname = strtok(files, ",");
    while(pathname != NULL) {
        abs_pathname = realpath(pathname, NULL);

        if(abs_pathname == NULL) {
            abs_pathname = malloc((UNIX_PATH_MAX + 1) * sizeof(char));
            getcwd(abs_pathname, UNIX_PATH_MAX + 1);

            strcat(abs_pathname, "/");
            if(pathname[0] == '.' && pathname[1] == '/') {
                strcat(abs_pathname, pathname + 2);
            } else {
                strcat(abs_pathname, pathname);
            }
        }

        if(openFile(abs_pathname, O_LOCK) != -1) {
            result = removeFile(abs_pathname);
        } else {
            result = -1;
        }

        if(p) {
            if(result == -1) {
                switch(errno) {
                    case EINVAL:
                        printf("-c %s: Errore, un errore sconosciuto è avvenuto, riprova più tardi\n", abs_pathname);

                        break;
                    case ENOENT:
                        printf("-c %s: Errore, il file non esiste, verifica il filename e riprova\n", abs_pathname);

                        break;
                    case EACCES:
                        printf("-c %s: Errore, la lock è posseduta da un altro utente, riprova più tardi\n", abs_pathname);

                        break;
                    case EPERM:
                        printf("-c %s: Errore, non possiedi la lock su questo file\n", abs_pathname);
                        break;
                }
            } else {
                printf("-c %s: Successo\n", pathname);
            }
        }

        if(timeout != 0) {
            usleep(timeout);
        }

        pathname = strtok(NULL, ",");

        free(abs_pathname);
    }

    return 0;
}