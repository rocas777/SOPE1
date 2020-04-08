#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>

#define DIRECTORY "."
#define DIRECTORY_LINE "./"
#define MAX_SIZE 1000         // Maximum address size (not efficient)
#define DIGITS_MAX 11         // Maximum number of digits
#define IGNORE_1 "."          // Used to ignore the current directory address
#define IGNORE_2 ".."         // Used to ignore the previous directory address
#define VERBOSE 0             // DEBUGGING
#define CUSTOM_INF 2147483647 // INFINITY (No need to call library math.h)

#define BLOCK_SIZE_STAT 512
#define BLOCK_SIZE_PRINT 1024

int num_sons = 0;

pid_t *pgid; //process id array for the parent of child's process group.
int num_pgid; //number of process groups

int **fd_arr; //pipe array
int num_fd; //number of pipes

struct timeval *startTime; //Time of beginning of program

FILE *log_filename; //File

/* Global variables: Used the dictate the flags */

typedef struct {
    int allFiles_C;     // -a or --all                          (C)
    int bytesDisplay_C; // -b, --bytes                          (C)
    int blockSize_C;    // -B or --block-size = SIZE            (C)
    int countLink_C;    // -l, --count-links                    // Always activated
    int dereference_C;  // -L, --dereference                    (C)
    int separatedirs_C; // -S, --separate-dirs                  (C)
    int maxDepth_C;     // --max-depth = N                      (C)
} cTags;

cTags tags = {0};

// Function prototypes:

// int num_thread = 0;
long long int seekdirec(char *currentdir, int depth);

void createProcess(char *currentdir, int depth, int fd[2]);

void printTags();

long long int sizeAttribution(struct stat *temp);

long long int dereferenceLink(char *workTable, int depth);

void sigintHandler(int sig);

void initSigaction();

void initSigactionSIGIGN();

void printInstantPid(pid_t *pid);

double timeSinceStartTime();

int argcG;
char **argvG;

// Function bodies:

/* Log prints*/
double timeSinceStartTime() //Returns time in milliseconds since begging of program
{
    struct timeval instant;
    gettimeofday(&instant, 0);

    return (double) (instant.tv_sec - startTime->tv_sec) * 1000.0f + (instant.tv_usec - startTime->tv_usec) / 1000.0f;
}

void printInstantPid(pid_t *pid) {
    fprintf(log_filename, "%0.2f - %d - ", timeSinceStartTime(), *pid);
    fflush(log_filename);
}

void printActionInfoCREATE(pid_t *pid, int argc, char *argv[]) {
    printInstantPid(pid);
    fprintf(log_filename, "CREATE - ");
    int i = 1;
    for (; i < argc - 1; i++) {
        fprintf(log_filename, "%s,", argv[i]);
    }
    fprintf(log_filename, "%s\n", argv[i]);
    fflush(log_filename);
}

void printActionInfoEXIT(pid_t *pid, int exit) {
    printInstantPid(pid);
    fprintf(log_filename, "EXIT - %i\n", exit);
    fflush(log_filename);
}

void printActionInfoRECV_SIGNAL(pid_t *pid, char *signal) {
    printInstantPid(pid);
    fprintf(log_filename, "RECV_SIGNAL - %s\n", signal);
    fflush(log_filename);
}

void printActionInfoSEND_SIGNAL(pid_t *pid, char *signal, pid_t destination) {
    printInstantPid(pid);
    fprintf(log_filename, "SEND_SIGNAL - %s \"%d\"\n", signal, destination);
    fflush(log_filename);
}

void printActionInfoRECV_PIP(pid_t *pid, char *message) {
    printInstantPid(pid);
    fprintf(log_filename, "RECV_PIP - %s\n", message);
    fflush(log_filename);
}

void printActionInfoSEND_PIPE(pid_t *pid, char *message) {
    printInstantPid(pid);
    fprintf(log_filename, "SEND_PIPE - %s\n", message);
    fflush(log_filename);
}

void printActionInfoENTRY(pid_t *pid, long long int bytes, char path[]) {
    printInstantPid(pid);
    fprintf(log_filename, "ENTRY - %lld %s\n", bytes, path);
    fflush(log_filename);
}

/* Accesses the link */

int blockSize = 512;

long long int dereferenceLink(char *workTable, int depth) {
    struct stat status;
    long long int sizeRep;

    char megaPath[MAX_SIZE];
    readlink(workTable, megaPath, MAX_SIZE - 1);
    if (!lstat(megaPath, &status)) {
        sizeRep = sizeAttribution(&status);
        if (depth > 0) {
            //printf("[%d]\t", depth);
            printf("%lld\t%s\n", sizeRep, workTable);
    	    fflush(stdout);
        }
        return sizeRep;
    } else {
        perror("(dereferenceLink) Failure to access path : ");
        return -1;
    }
}

/* [Used mostly for debugging] - Prints tags */
void printTags() {
    printf("Printing variables:\n");
    fflush(stdout);
    printf("tags.allFiles_C = %d\n", tags.allFiles_C);
    fflush(stdout);
    printf("tags.bytesDisplay_C = %d\n", tags.bytesDisplay_C);
    fflush(stdout);
    printf("tags.blockSize_C = %d\n", tags.blockSize_C);
    fflush(stdout);
    printf("tags.countLink_C = %d\n", tags.countLink_C);
    fflush(stdout);
    printf("tags.dereference_C = %d\n", tags.dereference_C);
    fflush(stdout);
    printf("tags.separatedirs_C = %d\n", tags.separatedirs_C);
    fflush(stdout);
    printf("tags.maxDepth_C = %d\n", tags.maxDepth_C);
    fflush(stdout);
}

/* Signals */
void sigintHandler(int sig) {
    pid_t pid = getpid();

    printActionInfoRECV_SIGNAL(&pid, "SIGINT");

    if (num_pgid != 0) {
        for (int n = 0; n < num_pgid; n++) {
            printActionInfoSEND_SIGNAL(&pid, "SIGSTOP", -pgid[n]);
            kill(-pgid[n], SIGSTOP);
        }
    }

    char i[2];
    while (i[0] != '0' && i[0] != '1') {
        printf("0 - Continue \n1 - Terminate Program\n\n");
        //int fd=(int)(stdin);
        read(0, i, 2);
    }

    if (i[0] == '0') {
        if (num_pgid != 0) {
            for (int n = 0; n < num_pgid; n++) {
                printActionInfoSEND_SIGNAL(&pid, "SIGCONT", -pgid[n]);
                kill(-pgid[n], SIGCONT);
            }
        }
    } else {
        if (num_pgid != 0) {
            for (int n = 0; n < num_pgid; n++) {
                printActionInfoSEND_SIGNAL(&pid, "SIGTERM", -pgid[n]);
                kill(-pgid[n], SIGTERM);
            }
        }
        printActionInfoSEND_SIGNAL(&pid, "SIGTERM", pid);
        kill(pid, SIGTERM);
    }
}

void initSigactionSIG_IGN() {
    // prepare the 'sigaction struct'
    struct sigaction action;
    action.sa_handler = SIG_IGN;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    // install the handler
    sigaction(SIGINT, &action, NULL);
}

void initSigaction() {
    // prepare the 'sigaction struct'
    struct sigaction action;
    action.sa_handler = sigintHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    // install the handler
    sigaction(SIGINT, &action, NULL);
}

/* Creates a process. */
void createProcess(char *currentdir, int depth, int fd[2]) {
    fflush(stdout);
    long long int n;
    pid_t pid;
    char digitsre[DIGITS_MAX];
    memset(digitsre, '\0', DIGITS_MAX);

    if ((pid = fork()) < 0) {
        fprintf(stderr, "fork error\n");
    	fflush(stderr);
        exit(2);
    } else if (pid > 0) { /* pai */
        if (close(fd[1]) < 0) { /* fecha lado emissor do pipe */
            perror("Failed to close write end of pipe");
        }

        if (num_pgid != -1) { // se não tiver pai (thread principal)
            pgid[num_pgid] = pid;
            num_pgid++;
        }

    } else { /* filho */
        if (close(fd[0]) < 0) { /* fecha lado receptor do pipe */
            perror("Failed to close read end of pipe");
        }
        fd_arr = (int **) malloc(sizeof(int *) * 1000 + sizeof(int) * 2 * 1000);
        int *ptr = (int *) (fd_arr + 1000);
        for (int i = 0; i < 1000; i++) {
            fd_arr[i] = ptr + i * 2;
        }

        num_fd = 0;
        initSigactionSIG_IGN();
        pid_t pid_p = getpid();
        if (num_pgid >= 0) {
            setpgrp(); // cria novo grupo de processos para o filho e filhos deste
        }
        num_pgid = -1;
        printActionInfoCREATE(&pid_p, argcG, argvG);


        depth--;
        n = seekdirec(currentdir, depth);
        sprintf(digitsre, "%lld", n);
        write(fd[1], digitsre, strlen(digitsre));
        printActionInfoSEND_PIPE(&pid_p, digitsre);

        //        printf("FILHO: %d fd0:\t%p\t%d\t", pid_p,(void *) fd, fd[0]);
        //        printf("valor: %lld\n", n);


        if (close(fd[1]) < 0) { /* fecha lado emissor do pipe */
            perror("Failed to close write end of pipe");
        }
        fflush(stdout);

        exit(0);
    }
}

void print(long long int size, char *workTable) {
    long long int out = size;
    size /= tags.blockSize_C;
    if (out % tags.blockSize_C)
        size++;

    printf("%lld\t%s\n", size, workTable);
    fflush(stdout);
}

/* Attributes sizes based on the activated tags. */
long long int sizeAttribution(struct stat *temp) {

    long long int value;

    if (tags.bytesDisplay_C)
        value = temp->st_size;
    else {
        value = temp->st_blocks * BLOCK_SIZE_STAT;
    }
    return value;
}

/* Reads all files in a given directory and displays identically the way 'du' does */
long long int seekdirec(char *currentdir, int depth) {
    pid_t pid_p = getpid();

    if (VERBOSE){
        printf("    [INFO] Received address '%s' ....... OK!\n", currentdir);
    	fflush(stdout);

    }

    char workTable[MAX_SIZE];
    memset(workTable, '\0', MAX_SIZE);
    strcpy(workTable, currentdir);

    DIR *d;
    struct dirent *dira;
    struct stat status;
    long long int size = 0;

    // First step: Attribute base size of the directory
    if (!lstat(workTable, &status)) {
        size += sizeAttribution(&status);
    }

    d = opendir(workTable);

    if (d == NULL) {
        perror("Error: ");
        return -1;
    } else {
        if (VERBOSE){
            printf("    [INFO] Directory '%s' was successfully loaded ....... OK!\n", workTable);
    fflush(stdout);}

        while ((dira = readdir(d)) != NULL) {
            //            sleep(1);

            strcpy(workTable, currentdir);
            strcat(workTable, "/");
            if (strcmp(dira->d_name, IGNORE_1) != 0 && strcmp(dira->d_name, IGNORE_2) != 0) {
                if (!lstat(strcat(workTable, dira->d_name), &status)) {
                    //printf("modo: %i\n",status.st_mode);
                    //printf("modo: %li\n",status.st_size);
                    // [DEBUG] printf("Cycle loaded by process id: %d -- (%s)!\n", getpid(), workTable);
                    // If it is a directory:
                    if (S_ISDIR(status.st_mode)) {
                        if (VERBOSE)
                            printf("    [INFO] Directory '%s' is a directory ....... OK! (size: %ld)\n", workTable,
                                   status.st_size);

                        if (pipe(fd_arr[num_fd]) < 0) {
                            fprintf(stderr, "pipe error\n");
    			    fflush(stderr);
                            exit(1);
                        }
                        int *fd;
                        fd = fd_arr[num_fd];

                        createProcess(workTable, depth, fd);

                        num_fd++;


			if(num_fd>0)
                        while (num_fd>100000) {
                            num_fd--;

                            int status = -1;
                            while (status != 0) {
                                int pid = wait(&status);
                                printActionInfoEXIT(&pid, status);
                                //            printf("num_fd:\t%d\t", num_fd);
                            }
                            num_pgid--;

                            long long int returned = 0;
                            char digitsre[DIGITS_MAX];
                            memset(digitsre, '\0', DIGITS_MAX);

                            read(fd_arr[num_fd][0], digitsre, DIGITS_MAX);
                            printActionInfoRECV_PIP(&pid_p, digitsre);
                            returned = atoll(digitsre);

                            if (close(fd_arr[num_fd][0]) < 0) { /* fecha lado receptor do pipe */
                                perror("Failed to close read end of pipe");
                            }

                            if (!tags.separatedirs_C)
                                size += returned;
                        }

                    }
                    // If it is a link:
                    else if (S_ISLNK(status.st_mode)) {
                        if (VERBOSE)
                            printf("    [INFO] Directory '%s' is a symbolic link ....... OK!\n", workTable);
                        if (tags.dereference_C == 0) {
                            // Only considers the file's own size
                            if (depth > 0) {
                                long long int temporary = sizeAttribution(&status);
                                if (tags.allFiles_C) {
                                    //printf("[%d]\t", depth);
                                    print(temporary, workTable);
                                }
                                printActionInfoENTRY(&pid_p, temporary, workTable);
                            }
                            size += sizeAttribution(&status);
                        } else {
                            // Considers the referenced file!
                            size += dereferenceLink(workTable, depth);
                        }
                    }
                    // If it is a regular file:
                    else if (S_ISREG(status.st_mode)) {
                        if (VERBOSE)
                            printf("    [INFO] Directory '%s' is a regular file ....... OK!\n", workTable);
                        size += sizeAttribution(&status);
                        if (depth > 0) {
                            long long int temporary = sizeAttribution(&status);
                            if (tags.allFiles_C) {
                                //printf("[%d]\t", depth);
                                //printf("acom: %lli\n",size);
                                //printf("file: %li\n",status.st_size);
                                print(temporary, workTable);
                            }
                            printActionInfoENTRY(&pid_p, temporary, workTable);
                        }
                    } else {
                        if (VERBOSE)
                            printf("    [INFO] Directory '%s' is a regular file ....... OK!\n", workTable);
                        size += sizeAttribution(&status);
                        if (depth > 0) {
                            long long int temporary = sizeAttribution(&status);
                            if (tags.allFiles_C) {
                                print(temporary, workTable);
                            }
                            printActionInfoENTRY(&pid_p, temporary, workTable);
                        }
                    }
                } else {
                    if (VERBOSE)
                        printf("    [FAILURE REPORT] Directory '%s' returned some error ....... ERROR!\n", workTable);
                }
            }
        }//fazer função wait processes e adiocionar aqui a informação
    }

    while (num_fd > 0) {
        num_fd--;

        int status = -1;
        while (status != 0) {
            int pid = wait(&status);
            printActionInfoEXIT(&pid, status);
            //            printf("num_fd:\t%d\t", num_fd);
        }
        num_pgid--;

        long long int returned = 0;
        char digitsre[DIGITS_MAX];
        memset(digitsre, '\0', DIGITS_MAX);

        read(fd_arr[num_fd][0], digitsre, DIGITS_MAX);
        printActionInfoRECV_PIP(&pid_p, digitsre);
        returned = atoll(digitsre);

        if (close(fd_arr[num_fd][0]) < 0) { /* fecha lado receptor do pipe */
            perror("Failed to close read end of pipe");
        }

        if (!tags.separatedirs_C)
            size += returned;
    }

    closedir(d);
    strcpy(workTable, currentdir);

    if (depth >= 0) {
        print(size, workTable);
        printActionInfoENTRY(&pid_p, size, workTable);
    }

    int status_exit;
    /*
    printf("Threads: %i\n",num_thread);
    while(num_thread){
        pid_t pid=wait(&status_exit);
        printActionInfoEXIT(&pid,status_exit);
        num_thread--;
    }*/
    //printf("dir: %lli\n",size);
    return size;
}

int main(int argc, char *argv[]) {
    startTime = malloc(sizeof(struct timeval));
    gettimeofday(startTime, 0);

    char directoryLine[MAX_SIZE] = DIRECTORY;
    tags.maxDepth_C = CUSTOM_INF;
    tags.countLink_C = 1;
    tags.blockSize_C = 1024;

    argvG = argv;
    argcG = argc;

    pgid = (int *) malloc(sizeof(int) * 1000);
    num_pgid = 0;

    fd_arr = (int **) malloc(sizeof(int *) * 1000 + sizeof(int) * 2 * 1000);
    int *ptr = (int *) (fd_arr + 1000);
    for (int i = 0; i < 1000; i++) {
        fd_arr[i] = ptr + i * 2;
    }
    num_fd = 0;

    initSigaction();

    if ((log_filename = fopen("/LOG_FILENAME", "w")) == NULL) {
        int errsv = errno;
        printf("%d\n", errsv);
    	fflush(stdout);
        perror(strerror(errsv));
        exit(1);
    }

    // Set up flags
    for (int i = 1; i < argc; i++) {
        // allFiles_C (-a or --all)
        if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--all") == 0) {
            tags.allFiles_C = 1;
        } else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--bytes") == 0) {
            tags.blockSize_C = 1;
            tags.bytesDisplay_C = 1;
        } else if (strcmp(argv[i], "-B") == 0 || strncmp(argv[i], "--block-size=", 13) == 0 ||
                   (strstr(argv[i], "-B")) != NULL || (strstr(argv[i], "--block-size=")) != NULL) {
            if (strcmp(argv[i], "-B") == 0) {
                i++;
                tags.blockSize_C = atoi(argv[i]);
            }
            if (strncmp(argv[i], "--block-size=", 13) == 0) {
                tags.blockSize_C = atoi(argv[i] + 13 * sizeof(char));
            }

            char *temp;
            if ((temp = strstr(argv[i], "-B")) != NULL) {
                temp += 2;
                tags.blockSize_C = atoi(temp);
            }
            if ((temp = strstr(argv[i], "--block-size=")) != NULL) {
                temp += 13;
                tags.blockSize_C = atoi(temp);
            }
        } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--count-links") == 0) {
        } else if (strcmp(argv[i], "-L") == 0 || strncmp(argv[i], "--dereference", 13) == 0) {
            tags.dereference_C = 1;
        } else if (strcmp(argv[i], "-S") == 0 || strncmp(argv[i], "--separate-dirs", 13) == 0) {
            tags.separatedirs_C = 1;
        } else if (strncmp(argv[i], "--max-depth=", 12) == 0) {
            tags.maxDepth_C = atoi(argv[i] + 12 * sizeof(char));
        } else {
            strcpy(directoryLine, argv[i]);
        }
    }

    //printTags();
    seekdirec(directoryLine, tags.maxDepth_C);

    fclose(log_filename);

    return 0;
}
