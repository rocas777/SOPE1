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

pid_t pgid; //process id for the parent of child's process group.

struct timeval *startTime; //Time of beggining of program

FILE *log_filename; //File

/* Global variables: Used the dictate the flags */

typedef struct
{
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
long long int createProcess(char *currentdir, int depth);
void printTags();
long long int sizeAttribution(struct stat *temp);
long long int dereferenceLink(char *workTable, int depth);
void sigintHandler(int sig);
void initSigaction();
void initSigactionSIGIGN();
void printInsantPid(pid_t *pid);
double timeSinceStartTime();
int argcG;
char **argvG;

// Function bodies:

/* Log prints*/
double timeSinceStartTime() //Returns time in miliseconds since begging of program
{
    struct timeval instant;
    gettimeofday(&instant, 0);

    return (double)(instant.tv_sec - startTime->tv_sec) * 1000.0f + (instant.tv_usec - startTime->tv_usec) / 1000.0f;
}

void printInsantPid(pid_t *pid)
{
    fprintf(log_filename, "%0.2f -\t %d -\t ", timeSinceStartTime(), *pid);
}

void printActionInfoCREATE(pid_t *pid, int argc, char *argv[])
{
    printInsantPid(pid);
    fprintf(log_filename, "CREATE -\t");
    int i = 1;
    for (; i < argc - 1; i++)
    {
        fprintf(log_filename, "%s,", argv[i]);
    }
    fprintf(log_filename, "%s\n", argv[i]);
    fflush(log_filename);
}

void printActionInfoEXIT(pid_t *pid, int exit)
{
    printInsantPid(pid);
    fprintf(log_filename, "EXIT -\t\t %i\n", exit);
    fflush(log_filename);
}

void printActionInfoRECV_SIGNAL(pid_t *pid, char *signal)
{
    printInsantPid(pid);
    fprintf(log_filename, "RECV_SIGNAL -\t %s\n", signal);
    fflush(log_filename);
}

void printActionInfoSEND_SIGNAL(pid_t *pid, char *signal, pid_t destination)
{
    printInsantPid(pid);
    fprintf(log_filename, "SEND_SIGNAL -\t %s \"%d\"\n", signal, destination);
    fflush(log_filename);
}

void printActionInfoRECV_PIP(pid_t *pid, char *message)
{
    printInsantPid(pid);
    fprintf(log_filename, "RECV_PIP -\t %s\n", message);
    fflush(log_filename);
}

void printActionInfoSEND_PIPE(pid_t *pid, char *message)
{
    printInsantPid(pid);
    fprintf(log_filename, "SEND_PIPE -\t %s\n", message);
    fflush(log_filename);
}

void printActionInfoENTRY(pid_t *pid, int argc, char *argv[])
{
    printInsantPid(pid);
    fprintf(log_filename, "ENTRY -\t ");
    fflush(log_filename);
}

/* Accesses the link */

int blockSize = 512;

long long int dereferenceLink(char *workTable, int depth)
{
    struct stat status;
    long long int sizeRep;

    char megaPath[MAX_SIZE];
    readlink(workTable, megaPath, MAX_SIZE - 1);
    if (!lstat(megaPath, &status))
    {
        sizeRep = sizeAttribution(&status);
        if (depth > 0)
        {
            //printf("[%d]\t", depth);
            printf("%lld\t%s\n", sizeRep, workTable);
        }
        return sizeRep;
    }
    else
    {
        perror("(dereferenceLink) Failure to access path : ");
        return -1;
    }
}

/* [Used mostly for debugging] - Prints tags */
void printTags()
{
    printf("Printing variables:\n");
    printf("tags.allFiles_C = %d\n", tags.allFiles_C);
    printf("tags.bytesDisplay_C = %d\n", tags.bytesDisplay_C);
    printf("tags.blockSize_C = %d\n", tags.blockSize_C);
    printf("tags.countLink_C = %d\n", tags.countLink_C);
    printf("tags.dereference_C = %d\n", tags.dereference_C);
    printf("tags.separatedirs_C = %d\n", tags.separatedirs_C);
    printf("tags.maxDepth_C = %d\n", tags.maxDepth_C);
}

/* Signals */
void sigintHandler(int sig)
{
    pid_t pid = getpid();

    printActionInfoRECV_SIGNAL(&pid, "SIGINT");

    if (pgid != 0)
    {
        printActionInfoSEND_SIGNAL(&pid, "SIGSTOP", -pgid);
        kill(-pgid, SIGSTOP);
    }

    int i = -1;
    while (i != 0 && i != 1)
    {
        printf("0 - Continue \n 1 - Terminate Program\n");
        scanf("%d", &i);
    }

    if (i == 0)
    {
        if (pgid != 0)
        {
            printActionInfoSEND_SIGNAL(&pid, "SIGCONT", -pgid);
            kill(-pgid, SIGCONT);
        }
    }
    else
    {
        if (pgid != 0)
        {
            printActionInfoSEND_SIGNAL(&pid, "SIGTERM", -pgid);
            kill(-pgid, SIGTERM);
        }
        printActionInfoSEND_SIGNAL(&pid, "SIGTERM", pid);
        kill(pid, SIGTERM);
    }
}

void initSigactionSIG_IGN()
{
    // prepare the 'sigaction struct'
    struct sigaction action;
    action.sa_handler = SIG_IGN;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    // install the handler
    sigaction(SIGINT, &action, NULL);
}

void initSigaction()
{
    // prepare the 'sigaction struct'
    struct sigaction action;
    action.sa_handler = sigintHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    // install the handler
    sigaction(SIGINT, &action, NULL);
}

/* Creates a process. */
long long int createProcess(char *currentdir, int depth)
{
    fflush(stdout);
    long long int n;
    int fd[2];
    pid_t pid;
    char digitsre[DIGITS_MAX];
    memset(digitsre, '\0', DIGITS_MAX);

    if (pipe(fd) < 0)
    {
        fprintf(stderr, "pipe error\n");
        exit(1);
    }

    if ((pid = fork()) < 0)
    {
        fprintf(stderr, "fork error\n");
        exit(2);
    }
    else if (pid > 0)
    {                 /* pai */
        close(fd[1]); /* fecha lado emissor do pipe */

        if (pgid == 0) // se não tiver pai (thread principal)
        {
            // setpgid(pid, 0); // cria novo grupo de processos para o filho e filhos deste
            pgid = pid;
        }
        else
        {
            setpgid(pid, getpgrp());
        }

        // num_thread++;

        int status = -1;
        while (status != 0)
        {
            waitpid(pid, &status, 0);
            printActionInfoEXIT(&pid, status);
        }

        if (pgid == pid) // se o grupo de processos acabar, se for necessário, é preciso criar um novo.
        {
            printf("\nreset:\t%d\n", pgid);
            pgid = 0;
        }

        read(fd[0], digitsre, DIGITS_MAX);
        pid_t pid_p = getpid();
        printActionInfoRECV_PIP(&pid_p, digitsre);
        n = atoll(digitsre);
        close(fd[0]); /* fecha lado receptor do pipe */
    }
    else
    { /* filho */
        initSigactionSIG_IGN();
        pid_t pid_p = getpid();
        if (pgid == 0)
            setpgrp(); // cria novo grupo de processos para o filho e filhos deste
        pgid = -1;
        printActionInfoCREATE(&pid_p, argcG, argvG);
        close(fd[0]); /* fecha lado receptor do pipe */
        depth--;
        n = seekdirec(currentdir, depth);
        sprintf(digitsre, "%lld", n);
        write(fd[1], digitsre, strlen(digitsre));
        printActionInfoSEND_PIPE(&pid_p, digitsre);
        close(fd[1]); /* fecha lado emissor do pipe */

        exit(0);
    }

    return n;
}

void print(long long int size, char *workTable)
{
    //if(tags.bytesDisplay_C){
    long long int out = size;
    //printf("%i\n",tags.blockSize_C);
    size /= tags.blockSize_C;
    if (out % tags.blockSize_C)
        size++;
    //}
    // printf("%d\tgroup %d\tpgid %d\t", getpid(), getpgrp(), pgid);
    printf("%lld\t%s\n", size, workTable);
}

/* Attributes sizes based on the activated tags. */
long long int sizeAttribution(struct stat *temp)
{

    long long int value;

    if (tags.bytesDisplay_C)
        value = temp->st_size;
    else
    {
        value = temp->st_blocks * BLOCK_SIZE_STAT;
        //long long out = value;
        //printf("%i\n",tags.blockSize_C);
        //value /= tags.blockSize_C;
        //if(out%tags.blockSize_C)
        //	value++;
    }
    //printf("%lli\n",value);
    return value;
}

/* Reads all files in a given directory and displays identically the way 'du' does */
long long int seekdirec(char *currentdir, int depth)
{
    if (VERBOSE)
        printf("    [INFO] Received address '%s' ....... OK!\n", currentdir);

    char workTable[MAX_SIZE];
    memset(workTable, '\0', MAX_SIZE);
    strcpy(workTable, currentdir);

    DIR *d;
    struct dirent *dira;
    struct stat status;
    long long int size = 0;

    // First step: Attribute base size of the directory
    if (!lstat(workTable, &status))
    {
        size += sizeAttribution(&status);
    }

    d = opendir(workTable);

    if (d == NULL)
    {
        perror("Error: ");
        return -1;
    }
    else
    {
        if (VERBOSE)
            printf("    [INFO] Directory '%s' was successfully loaded ....... OK!\n", workTable);

        while ((dira = readdir(d)) != NULL)
        {
            strcpy(workTable, currentdir);
            strcat(workTable, "/");
            if (strcmp(dira->d_name, IGNORE_1) != 0 && strcmp(dira->d_name, IGNORE_2) != 0)
            {
                if (!lstat(strcat(workTable, dira->d_name), &status))
                {
                    //printf("modo: %i\n",status.st_mode);
                    //printf("modo: %li\n",status.st_size);
                    // [DEBUG] printf("Cycle loaded by process id: %d -- (%s)!\n", getpid(), workTable);
                    // If it is a directory:
                    if (S_ISDIR(status.st_mode))
                    {
                        if (VERBOSE)
                            printf("    [INFO] Directory '%s' is a directory ....... OK! (size: %ld)\n", workTable, status.st_size);
                        long long int returned = 0;
                        returned = createProcess(workTable, depth);

                        if (!tags.separatedirs_C)
                            size += returned;
                    }
                    // If it is a link:
                    else if (S_ISLNK(status.st_mode))
                    {
                        if (VERBOSE)
                            printf("    [INFO] Directory '%s' is a symbolic link ....... OK!\n", workTable);
                        if (tags.dereference_C == 0)
                        {
                            // Only considers the file's own size
                            if (tags.allFiles_C)
                            {
                                if (depth > 0)
                                {
                                    //printf("[%d]\t", depth);
                                    long long int temporary = sizeAttribution(&status);
                                    print(temporary, workTable);
                                }
                            }
                            size += sizeAttribution(&status);
                        }
                        else
                        {
                            // Considers the referenced file!
                            size += dereferenceLink(workTable, depth);
                        }
                    }
                    // If it is a regular file:
                    else if (S_ISREG(status.st_mode))
                    {
                        if (VERBOSE)
                            printf("    [INFO] Directory '%s' is a regular file ....... OK!\n", workTable);
                        size += sizeAttribution(&status);
                        if (tags.allFiles_C)
                        {
                            if (depth > 0)
                            {
                                //printf("[%d]\t", depth);
                                //printf("acom: %lli\n",size);
                                //printf("file: %li\n",status.st_size);
                                long long int temporary = sizeAttribution(&status);
                                print(temporary, workTable);
                            }
                        }
                    }
                    else
                    {
                        if (VERBOSE)
                            printf("    [INFO] Directory '%s' is a regular file ....... OK!\n", workTable);
                        size += sizeAttribution(&status);
                        if (tags.allFiles_C)
                        {
                            if (depth > 0)
                            {
                                long long int temporary = sizeAttribution(&status);
                                print(temporary, workTable);
                            }
                        }
                    }
                }
                else
                {
                    if (VERBOSE)
                        printf("    [FAILURE REPORT] Directory '%s' returned some error ....... ERROR!\n", workTable);
                }
            }
        }
    }
    closedir(d);
    strcpy(workTable, currentdir);

    if (depth >= 0)
    {
        print(size, workTable);
    }
    // sleep(1);
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

int main(int argc, char *argv[])
{
    startTime = malloc(sizeof(struct timeval));
    gettimeofday(startTime, 0);

    char directoryLine[MAX_SIZE] = DIRECTORY;
    tags.maxDepth_C = CUSTOM_INF;
    tags.countLink_C = 1;
    tags.blockSize_C = 1024;

    argvG = argv;
    argcG = argc;
    pgid = 0;

    initSigaction();

    if ((log_filename = fopen("../LOG_FILENAME", "w")) == NULL)
    {
        int errsv = errno;
        printf("%d\n", errsv);
        perror(strerror(errsv));
        exit(1);
    }

    // Set up flags
    for (int i = 1; i < argc; i++)
    {
        // allFiles_C (-a or --all)
        if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--all") == 0)
        {
            tags.allFiles_C = 1;
        }
        else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--bytes") == 0)
        {
            tags.blockSize_C = 1;
            tags.bytesDisplay_C = 1;
        }
        else if (strcmp(argv[i], "-B") == 0 || strncmp(argv[i], "--block-size=", 13) == 0 || (strstr(argv[i], "-B")) != NULL || (strstr(argv[i], "--block-size=")) != NULL)
        {
            if (strcmp(argv[i], "-B") == 0)
            {
                i++;
                tags.blockSize_C = atoi(argv[i]);
            }
            if (strncmp(argv[i], "--block-size=", 13) == 0)
            {
                tags.blockSize_C = atoi(argv[i] + 13 * sizeof(char));
            }

            char *temp;
            if ((temp = strstr(argv[i], "-B")) != NULL)
            {
                temp += 2;
                tags.blockSize_C = atoi(temp);
            }
            if ((temp = strstr(argv[i], "--block-size=")) != NULL)
            {
                temp += 13;
                tags.blockSize_C = atoi(temp);
            }
        }
        else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--count-links") == 0)
        {
            // Do thing
        }
        else if (strcmp(argv[i], "-L") == 0 || strncmp(argv[i], "--dereference", 13) == 0)
        {
            tags.dereference_C = 1;
        }
        else if (strcmp(argv[i], "-S") == 0 || strncmp(argv[i], "--separate-dirs", 13) == 0)
        {
            tags.separatedirs_C = 1;
        }
        else if (strncmp(argv[i], "--max-depth=", 12) == 0)
        {
            tags.maxDepth_C = atoi(argv[i] + 12 * sizeof(char));
        }
        else
        {
            strcpy(directoryLine, argv[i]);
        }
    }

    //printTags();
    seekdirec(directoryLine, tags.maxDepth_C);

    fclose(log_filename);

    return 0;
}
