#define UNIX_PATH_MAX 108

struct metadata {
    int fid;
    char filename[UNIX_PATH_MAX];
    int size;
    struct timespec last_used;
    int acquired_by;
    int *opened;
    struct f_el *next_file;
}

struct f_el {
    struct metadata metadata;
    char *data;
};

typedef struct metadata metadata;
typedef struct f_el f_el;

int insert(f_el *h, f_el *n_file);

int lookup(f_el *h, f_el *file);

int delete(f_el *h, f_el *file);

int hash(int fid, int n, int i);


int insert(f_el *h, f_el *n_file) {
    int index;
    int i;

    i = 0;

    while()

    
}

int lookup(f_el *h, f_el *file) {

}

int delete(f_el *h, f_el *file) {

}

int hash(int fid, int n, int i) {
    int h1 = fid % n;
    int h2 = (fid + i) % n;

    if(h2 % 2 == 0) {
        h2++;
    }

    return (h1 + (i * h2)) % n;
}