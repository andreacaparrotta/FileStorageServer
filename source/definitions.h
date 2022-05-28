// Definizione dei messaggi di richiesta
#define CLOSECONN "0"                               // È richiesta la chiusura della connessione
#define OPENFILE "1"                                // È richiesta l'apertura del file
#define CLOSEFILE "2"                               // È richiesta la chiusura del file
#define WRITEFILE "3"                               // È richiesta la scrittura in un file
#define READFILE "4"                                // È richiesta la lettura di un file
#define READNFILE "5"                               // È richiesta la lettura di n file dallo storage
#define APPENDFILE "6"                              // È richiesta la scrittura in coda al contenuto di un file
#define LOCKFILE "7"                                // È richiesta l'acquisizione della lock su un file
#define UNLOCKFILE "8"                              // È richiesto il rilascio della lock su un file
#define REMOVEFILE "9"                              // È richiesta la rimozione di un file dallo storage
#define WRITE_NO_CONTENT "10"                       // È richiesta la scrittura di un file senza contenuto

// Definizione dei messaggi di risposta
#define SUCCESS "0"                                 // L'operazione è terminata con successo
#define ALREADY_OPENED "1"                          // Il file è stato già aperto
#define FILE_NOT_EXIST "2"                          // Il file specificato non esiste
#define UNKNOWN "3"                                 // Errore sconosciuto
#define FILENAME_TOO_LONG "4"                       // Il nome del file è troppo lungo
#define FILE_ALREADY_EXIST "5"                      // Il file esiste già 
#define FILE_NOT_OPENED "6"                         // Il file non è stato aperto 
#define FILE_LOCKED "7"                             // Il file è locked e l'operazione è richiesta da un utente che non è in possesso della lock    
#define NOT_ENO_MEM "8"                             // Lo storage non è sufficiente per memorizzare il file
// Definizione flags per open_file
#define O_CREATE 1                                  // Crea il file se non esistente
#define O_LOCK 2                                    // Crea o apre il file in modalità locked
